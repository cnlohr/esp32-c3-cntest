#include <cnip_config.h>
#include <cnip_core.h>
#include "cnip_http.h"
#include "cnip_http_config.h"
#include <cnip_hal.h>
#include <stdio.h>
#include <mfs.h>

uint16_t htons(uint16_t hostshort);

//uint8_t * databuff_ptr;
//uint8_t   databuff[1536];
//int cork_binary_rx;

static int sent_packet = 0;

void cnip_user_init( cnip_hal * hal )
{
	//Assume tcpip_adapter takes care of most of the stuff, just need to init http.
	//XXX Also need to handle this elegantely with multiple interfaces.
	MFSInit();
	printf( "Networking user init\n" );
}

void CNIP_IRAM cnip_user_timed_tick( cnip_hal * hal, int is_slow )
{
	//XXX Also need to handle this elegantely with multiple interfaces.
	HTTPTick( is_slow );

#if 0
	if( is_slow )
	{
		int i;
		printf( "H:" );
		for( i = 0; i < 10; i++ )
		{
			printf( "%d ", HTTPConnections[i].state );
		}
		printf( "\n" );
	}
#endif
}


void CNIP_IRAM cnip_handle_udp( cnip_ctx * ctx, uint16_t len )
{
	//printf( "Got UDP packet!\n" );
	//If we want to do anything with UDP
}

uint8_t CNIP_IRAM cnip_tcp_recv_syn_cb( struct cnip_ctx_t * ctx, uint16_t portno )
{
	if( portno == 80 )
	{
		int rsck = cnip_tcp_get_free_connection( ctx );
		int httpsock = httpserver_connectcb( &ctx->TCPs[rsck] );
		ctx->TCPs[rsck].useri = httpsock;
		return rsck;
	}

	printf( "Got SYN on port %d\n", portno );
	return 0;
}

uint8_t CNIP_IRAM cnip_tcp_recv_data_cb( cnip_tcp * tcp, int sockno, uint16_t totallen )
{
	sent_packet = 0;
	http_recvcb( tcp->useri, (char*)tcp->ctx->hal->incoming_cur, totallen );
	return sent_packet;
}

void CNIP_IRAM cnip_tcp_connection_closing_cb( cnip_tcp * tcp, int sockno )
{
	http_disconnetcb( tcp->useri );
	return;
}


void CNIP_IRAM cnip_received_tcp_ack_cb( cnip_tcp * tcp, int sockno )
{
	if( tcp->useri )
	{
		HTTPTick( 0 );
	}
}




/* Functions needed to make cnhttp go! */

void CNIP_IRAM DataStartPacket()
{
	//printf( "Starting write: %p\n", curhttp->socket );
	((cnip_tcp*)curhttp->socket)->sendtype = PSHBIT | ACKBIT;
	cnip_tcp_start_write( curhttp->socket );
}

void CNIP_IRAM PushString( const char * data )
{
	cnip_hal_pushstr( ((cnip_tcp*)curhttp->socket)->ctx->hal, data );
}

void CNIP_IRAM PushByte( uint8_t c )
{
	cnip_hal_push8( ((cnip_tcp*)curhttp->socket)->ctx->hal, c );
}

void CNIP_IRAM PushBlob( const uint8_t * datam, int len )
{
	//cnip_hal * hal = ((cnip_tcp*)curhttp->socket)->ctx->hal;
	cnip_hal_pushblob( ((cnip_tcp*)curhttp->socket)->ctx->hal, datam, len );
}

uint8_t * CNIP_IRAM GetPtrToPushAndAdvance( int len )
{
	cnip_hal * hal = ((cnip_tcp*)curhttp->socket)->ctx->hal;
	uint8_t * ret = hal->outgoing_cur;
	hal->outgoing_cur += len;
	return ret;
}

int CNIP_IRAM TCPCanSend( cnhttp_socket socket, int size )
{
	return cnip_tcp_can_send( socket );
}

int CNIP_IRAM EndTCPWrite( cnhttp_socket socket )
{
	sent_packet = 1;
	cnip_tcp_end_write( socket );
	return 0;
}

int CNIP_IRAM TCPDoneSend( cnhttp_socket socket )
{
	return cnip_tcp_can_send( socket );
}

void CNIP_IRAM et_espconn_disconnect( cnhttp_socket socket )
{
	cnip_tcp_request_closure( socket );
}




void CNIP_IRAM Uint32To10Str( char * out, uint32_t dat )
{
	int tens = 1000000000;
	int val;
	int place = 0;

	while( tens > 1 )
	{
		if( dat/tens ) break;
		tens/=10;
	}

	while( tens )
	{
		val = dat/tens;
		dat -= val*tens;
		tens /= 10;
		out[place++] = val + '0';
	}

	out[place] = 0;
}

//from http://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
static const char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                      'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                      'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                      'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                      'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                      'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                      'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                      '4', '5', '6', '7', '8', '9', '+', '/'};

static const int mod_table[] = {0, 2, 1};

void CNIP_IRAM my_base64_encode(const unsigned char *data, unsigned int input_length, uint8_t * encoded_data )
{

	int i, j;
    int output_length = 4 * ((input_length + 2) / 3);

    if( !encoded_data ) return;
	if( !data ) { encoded_data[0] = '='; encoded_data[1] = 0; return; }

    for (i = 0, j = 0; i < input_length; ) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

	int mod_table_len = mod_table[input_length % 3];
    for (i = 0; i < mod_table_len; i++)
	{
        encoded_data[output_length - 1 - i] = '=';
	}
	encoded_data[j] = 0;
}

uint8_t CNIP_IRAM hex1byte( char c )
{
	if( c >= '0' && c <= '9' ) return c - '0';
	if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	return 0;
}

