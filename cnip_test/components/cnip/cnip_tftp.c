#include "cnip_tftp.h"
#include <esp_log.h>
#include <string.h>

static const char *TAG = "TFTP";

#ifndef ENABLE_CNIP_TFTP_SERVER
#else


#define TFTP_TYPE_RRQ   1
#define TFTP_TYPE_WRQ   2
#define TFTP_TYPE_DATA  3
#define TFTP_TYPE_ACK   4
#define TFTP_TYPE_ERROR 5


struct TFTPConnection
{
	FILE * file;
	uint32_t file_length;
	cnip_addy remote_ip;
	uint16_t remote_port;
	uint16_t last_id;
	uint8_t time_remaining;
	uint8_t mode_write;
} tftp_connections[MAX_TFTP_CONNECTIONS];

static void TFTPClose( struct TFTPConnection * c )
{
	if( c->file )
	{
		fclose( c->file );
		c->file = 0;
	}
	c->time_remaining = 0;
	c->last_id = 0;
}

static int AllocFile( cnip_ctx * ctx, const char * fname, int write )
{
	int i;
	for( i = 0; i < MAX_TFTP_CONNECTIONS; i++ )
	{
		if( tftp_connections[i].file == 0 )
		{
			FILE * f = tftp_connections[i].file = fopen( fname, write?"wb":"rb" );
			if( f )
			{
				struct TFTPConnection * c = &tftp_connections[i];
				fseek( f, 0, SEEK_END );
				c->file_length = ftell( f );
				fseek( f, 0, SEEK_SET );
				c->time_remaining = 0xff;
				c->last_id = 0;
				c->remote_ip = ctx->ipsource;
				c->remote_port = ctx->remoteport;
				c->mode_write = write;
				return i;
			}
			else
			{
				ESP_LOGE( TAG, "Error: Could not open filename %s (write=%d)\n", fname, write );
				return -2;
			}
		}
	}
	ESP_LOGE( TAG, "Error: No free slots in TFTP\n", fname );
	return -1;
}

static void CNIPTFTPSendAck( cnip_ctx * ctx, int connection, int id )
{
	uint16_t payload[2] = { CNIP_HTONS( TFTP_TYPE_ACK ), CNIP_HTONS( id ) };
	struct TFTPConnection * conn = &tftp_connections[connection];
	cnip_util_emit_udp_packet( ctx, (uint8_t*)payload, 4, conn->remote_ip, TFTP_LOCAL_BASE_PORT + connection, conn->remote_port );
}

static void CNIPTFTPReplyError( cnip_ctx * ctx, int id, const char * friendlyname, cnip_addy remote_ip, uint16_t local_port, uint16_t remote_port )
{
	uint16_t payload[64] = { CNIP_HTONS( TFTP_TYPE_ERROR ), CNIP_HTONS( id ) };
	int len = strlen( friendlyname ) + 1;
	if( len >= 120 ) len = 119;
	memcpy( payload+2, friendlyname, len );
	((uint8_t*)(payload+2))[len] = 0;
	cnip_util_emit_udp_packet( ctx, (uint8_t*)payload, len+4, remote_ip, local_port, remote_port );
}


static void CNIPTFTPSendData( cnip_ctx * ctx, struct TFTPConnection * conn )
{
	uint16_t payload[2+256] = { CNIP_HTONS( TFTP_TYPE_DATA ), CNIP_HTONS( conn->last_id + 1 ) };
	int this_id = conn->last_id;
	int tosend = conn->file_length - this_id*512;
	if( tosend < 0 ) tosend = 0;

	if( tosend > 0 )
	{
		if( tosend > 512 ) tosend = 512;
		fseek( conn->file, this_id * 512, SEEK_SET );
		fread( payload+2, tosend, 1, conn->file );
	}
	cnip_util_emit_udp_packet( ctx, (uint8_t*)payload, tosend+4, conn->remote_ip, (int)(conn-tftp_connections) + TFTP_LOCAL_BASE_PORT, conn->remote_port );
	if( tosend == 0 )
	{
		TFTPClose( conn );
	}
}


void HandleCNIPTFTPUDPPacket( cnip_ctx * ctx, int length )
{
	cnip_hal * hal = ctx->hal;
	uint16_t opcode = cnip_hal_pop16( hal );
	switch( opcode )
	{
		case TFTP_TYPE_RRQ:
		case TFTP_TYPE_WRQ:
		{
			if( ctx->localport != 69 )
			{
				CNIPTFTPReplyError( ctx, 1, "Attempted RQ on non control port.", ctx->ipsource, ctx->localport, ctx->remoteport );
				break;
			}

			char fname[256];
			char mode[256];
			//WRQ and RRQ are very similar.
			cnip_hal_popstr( hal, fname, sizeof( fname )-1 );
			cnip_hal_popstr( hal, mode, sizeof( mode )-1 );

			//Expect modes netascii, octet or mail.  Case insensitive.
			//We just treat everything as octet anyway.
			int f = AllocFile( ctx, fname, opcode == TFTP_TYPE_WRQ );
			if( f >= 0 )
			{
				if( opcode == TFTP_TYPE_WRQ )
				{
					struct TFTPConnection * con = tftp_connections + f;
					// Send ack.
					con->last_id = 0;
					CNIPTFTPSendAck( ctx, f, 0 );
				}
				else
				{
					struct TFTPConnection * con = tftp_connections + f;
					con->last_id = 0;
					//Is an RRQ.  Send some data.
					CNIPTFTPSendData( ctx, con );
				}
			}
			else
			{
				CNIPTFTPReplyError( ctx, 1, "File I/O Fail", ctx->ipsource, ctx->localport, ctx->remoteport );				
			}
			break;
		}
		case TFTP_TYPE_DATA:
		case TFTP_TYPE_ACK:
		{
			unsigned int connid = ctx->localport - TFTP_LOCAL_BASE_PORT;
			int valid = 0;
			if( connid < MAX_TFTP_CONNECTIONS )
			{
				struct TFTPConnection * conn = tftp_connections + connid;
				if( conn->time_remaining && conn->file && conn->remote_port == ctx->remoteport && conn->remote_ip == ctx->ipsource )
				{
					uint16_t blockno = cnip_hal_pop16( hal );
					if( conn->mode_write && opcode == TFTP_TYPE_DATA )
					{
						if( blockno == conn->last_id + 1 )
						{
							char payload[512];
							length -= 4;
							cnip_hal_popblob( hal, (uint8_t*)payload, length );
							fwrite( payload, length, 1, conn->file ); 
							conn->last_id++;
							valid = 1;
							CNIPTFTPSendAck( ctx, connid, blockno );
							if( length != 512 )
							{
								TFTPClose( conn );
							}
						}
						else if( blockno == conn->last_id )
						{
							//Duplicate 
							valid = 1;
						}
					}
					else if( !conn->mode_write && opcode == TFTP_TYPE_ACK )
					{
						if( blockno == conn->last_id + 1 )
						{
							conn->last_id = blockno;
							CNIPTFTPSendData( ctx, conn );					
							valid = 1;
						}
					}
				}
			}
			if( !valid )
			{
				CNIPTFTPReplyError( ctx, 1, "Invalid connection or connection state", ctx->ipsource, ctx->localport, ctx->remoteport );				
			}
			break;
		}
		case TFTP_TYPE_ERROR:
		{
			char ferr[256];
			cnip_hal_popstr( hal, ferr, sizeof( ferr )-1 );

			ESP_LOGE( TAG, "TFTP Received error %s\n", ferr );
			break;
		}
		default:
			ESP_LOGE( TAG, "TFTP Received unknown opcode %d\n", opcode );
			break;
	}
	cnip_hal_stopop( hal ); //Just in case.
}

void TickCNIP_TFTP()
{
	int i;
	for( i = 0; i < MAX_TFTP_CONNECTIONS; i++ )
	{
		struct TFTPConnection * c = &tftp_connections[i];
		if( c->time_remaining )
		{
			c->time_remaining--;
			if( c->time_remaining == 0 )
			{
				TFTPClose( c );
			}
		}
	}
}



#endif
