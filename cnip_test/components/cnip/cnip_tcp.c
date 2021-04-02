//Copyright 2012 <>< Charles Lohr, under the MIT-x11 or NewBSD license.  You choose.
//
//Remaining TODO
//
//* TCP Keepalive.
//* Handle FIN correctly.

#include <cnip_tcp.h>
#include <stdint.h>
#include <string.h>
#include <cnip_core.h>
#include <stdlib.h>

#if !defined( MUTE_PRINTF )
#include <stdio.h>
#define MARK(x...) printf(x)
#else
#define MARK(x,...)
#endif

#ifdef INCLUDE_TCP

struct cnip_tcp_header_t
{
	//Does not include ports.
	uint32_t seqno;
	uint32_t ackno;
	uint8_t  hlen_and_hflags;
	uint8_t  lflags;
	uint16_t windowsize;
	uint16_t checksum;
	uint16_t urgent;
};

struct cnip_tcp_header_plus_ports_t
{
	//Does not include ports.
	uint16_t srcport;
	uint16_t dstport;
	uint32_t seqno;
	uint32_t ackno;
	uint8_t  hlen_and_hflags;
	uint8_t  lflags;
	uint16_t windowsize;
	uint32_t checksum_and_urgent;
};

struct etherlink_header_l_t
{
	uint8_t remote_mac[6];
	uint8_t localmac[6];
	uint16_t proto;
};


int CNIP_IRAM GetAssociatedPacket( cnip_ctx * ctx )
{
	int8_t i;
	cnip_tcp * t = &ctx->TCPs[0];

	for( i = 1; i < TCP_SOCKETS; i++ )
	{
		t++;
		if( t->state && t->this_port == ctx->localport && t->dest_port == ctx->remoteport && t->dest_addr == ctx->ipsource )
		{
			return i;
		}
	}
	return 0;
}

static CNIP_IRAM void write_tcp_header( cnip_tcp * t )
{
	cnip_ctx * ctx = t->ctx;
	cnip_hal * hal = ctx->hal;

	cnip_hal_startsend( hal, t->sendptr, TCP_BUFFERSIZE );

	struct etherlink_header_l_t * tl = (struct etherlink_header_l_t*)
		cnip_hal_push_ptr_and_advance( hal, sizeof( struct etherlink_header_l_t  ) );
	memcpy( tl->remote_mac, t->remote_mac, 6 );
	memcpy( tl->localmac, hal->mac, 6 );
	tl->proto = CNIP_HTONS( 0x0800 ); 

	cnip_send_ip_header( ctx, 0x00, t->dest_addr, TCP_PROTOCOL_NUMBER ); //Size, etc. will be filled into IP header later.

	struct cnip_tcp_header_plus_ports_t * tcph = (struct cnip_tcp_header_plus_ports_t*)
		cnip_hal_push_ptr_and_advance( hal, sizeof( struct cnip_tcp_header_plus_ports_t ) );
	tcph->srcport = CNIP_HTONS( t->this_port );
	tcph->dstport = CNIP_HTONS( t->dest_port );
	tcph->seqno = CNIP_HTONL( t->seq_num );
	tcph->ackno = CNIP_HTONL( t->ack_num );
	tcph->hlen_and_hflags = (TCP_HEADER_LENGTH/4)<<4; //Data offset, reserved, NS flags all 0
	tcph->lflags = t->sendtype;
	tcph->windowsize = CNIP_HTONS( 2048 );
	tcph->checksum_and_urgent = 0;
}

static CNIP_IRAM void UpdateRemoteMAC( cnip_ctx * ctx, uint8_t * c )
{
	memcpy( c, ctx->macfrom, 6 );
}

//////////////////////////EXTERNAL CALLABLE FUNCTIONS//////////////////////////

void CNIP_IRAM cnip_tcp_init( cnip_ctx * ctx )
{
	uint8_t i;
	if( !ctx->TCPs )
	{
		ctx->TCPs = malloc( sizeof( cnip_tcp ) * TCP_SOCKETS );
	}
	for( i = 0; i < TCP_SOCKETS; i++ )
	{
		cnip_tcp * t = &ctx->TCPs[i];
		t->state = 0;
		t->ctx = ctx;
	}
}


void CNIP_IRAM cnip_tcp_handle( cnip_ctx * ctx, uint16_t iptotallen)
{
	cnip_hal * hal = ctx->hal;
	// ipsource is a global
	int rsck = GetAssociatedPacket( ctx);
	cnip_tcp * t = &ctx->TCPs[rsck];

	struct cnip_tcp_header_t * h = (struct cnip_tcp_header_t *)cnip_hal_pop_ptr_and_advance( hal, sizeof(struct cnip_tcp_header_t) );

	if( !h ) return;

	int did_ack_ok = 0; //Will get set to 1 if we got an ack.

	uint32_t sequence_num = CNIP_HTONL( h->seqno );
	uint32_t ack_num = CNIP_HTONL( h->ackno );
	uint8_t hlen = (h->hlen_and_hflags>>2) & 0xFC;
	uint8_t flags = h->lflags;

	iptotallen -= hlen;

	//Clear out rest of header
	cnip_hal_dumpbytes( hal, hlen - 20 );

#ifdef TCP_HAS_SERVER
	if( flags & SYNBIT )	//SYN
	{
		cnip_tcp * t;
		int8_t rsck = cnip_tcp_recv_syn_cb( ctx, ctx->localport );

		if( rsck == 0 ) goto reset_conn0;
		t = &ctx->TCPs[rsck];

		UpdateRemoteMAC( ctx, t->remote_mac );

		t->this_port = ctx->localport;
		t->dest_port = ctx->remoteport;
		t->dest_addr = ctx->ipsource;
		t->ack_num = sequence_num + 1;
		t->seq_num = 0; //should be random?
		t->state = ESTABLISHED;
		t->time_since_sent = 0;
		t->idletime = 0;

		//Don't forget to send a syn,ack back to the host.

		cnip_hal_finish_callback_now( hal );

		t->sendtype = SYNBIT | ACKBIT; //SYN+ACK
		cnip_tcp_start_write( t );

		t->seq_num++;
//		t->seq_num = t->seq_num;

		//The syn overrides everything, so we can return.

		//MARK( "__syn %d\n", rsck);
		goto end_and_emit;
	}
#endif

	UpdateRemoteMAC( ctx, t->remote_mac );

	//If we don't have a real connection, we're going to have to bail here.
	if( !rsck )
	{
		//XXX: Tricky: Do we have to reject spurious ACK packets?  Perhaps they are from keepalive
		if( flags & ( PSHBIT | SYNBIT | FINBIT ) )
		{
			//MARK( "__reset0" );
			goto reset_conn0;
		}
		else
		{
			cnip_hal_finish_callback_now( hal );
			return;
		}
	}

	if( flags & RSTBIT)
	{
		cnip_tcp_connection_closing_cb( t, rsck );
		t->sendtype = RSTBIT | ACKBIT;
		t->state = 0;
		//MARK( "__rstbit %d\n", rsck );
		goto send_early;
	}

	//We have an associated connection on T.

	if( flags & ACKBIT ) //ACK
	{
		//t->sendlength - 34 - 20
		uint16_t payloadlen = t->sendlength - 34 - 20;
//		printf( "ACKLEN: %d\n", payloadlen );
		uint32_t nextseq = t->seq_num;
		if( payloadlen == 0)
			nextseq ++;               //SEQ NUM
		else
			nextseq += payloadlen;   //SEQ NUM

		int16_t diff = ack_num - nextseq;// + iptotallen; // I don't know about this part)
//		printf( "%d / %d\n", ack_num, t->seq_num );

		//Lost a packet...
		if( diff < 0 )
		{
			//MARK( "__badack %d %d\n", diff, rsck );
			//This is an interesting one...  If we get acked for something before we would send this gets flagged.
			//This is actually common on opening/closing connections.
		}
		//XXX TODO THIS IS PROBABLY WRONG  (was <=, meaning it could throw away packets)
		else if( diff == 0 )
		{
			//t->seq_num = ack_num; //SEQ_NUM  This seems more like a bandaid.
			//The above line seems to be useful for bandaiding errors, when doing a more complete check, uncomment it.
			t->idletime = 0;
			t->time_since_sent = 0;

			//printf( "Setting next seq: %d\n", nextseq );
			t->seq_num = nextseq;
			did_ack_ok = 1;
			//t->seq_num = t->seq_num;
//			MARK( "__goodack" );
		}
	}

	if( flags & FINBIT )
	{
		if( t->state == CLOSING_WAIT )
		{
			t->sendtype = ACKBIT;
			t->ack_num = sequence_num+1;
			t->state = 0;
			MARK( "__clodone %d", rsck );
			goto send_early;
		}
		else
		{
			//XXX TODO Handle FIN correctly.
			t->ack_num++;              //SEQ NUM
			t->sendtype = ACKBIT | FINBIT;
			t->state = CLOSING_WAIT;
			cnip_tcp_connection_closing_cb( t, rsck );
			//MARK( "__finbit %d\n", rsck );
			goto send_early;
		}
	} else if( t->state == CLOSING_WAIT )
	{
		t->state = 0;
		goto send_early;
	}




	//XXX: TODO: Consider if time_since_sent is still set...
	//This is because if we have a packet that was lost, we can't plough over it with an ack.
	//If we do, the packet will be lost, and we will be out-of-sync.

	//TODO: How do we handle PDU's?  They are sent across multiple packets.  Should we supress ACKs until we have a full PDU?
	//if( flags & PSHBIT ) //PSH 
	if( iptotallen )
	{
		//printf( "Checking: %08x %08x\n", t->ack_num, sequence_num );
		uint32_t seq_diff = t->ack_num - sequence_num;

		if( seq_diff <= iptotallen )
		{
			//printf( "SEQD: %d\n", seq_diff );

			t->ack_num = sequence_num + iptotallen;

			//XXX Tricky: We need to handle the situation where we missed a packet from the peer and
			//they appended more to a packet they missed an ack from.  If seq_diff is zero, it means
			//we care about the whole packet.  Otherwise we care about a subset.
			cnip_hal_dumpbytes( hal, seq_diff );
			iptotallen -= seq_diff;

			if( !cnip_tcp_recv_data_cb( t, rsck, iptotallen ) )
			{
				//The callback did not send a packet w/ack.
				if( t->time_since_sent )
				{
					//hacky, we need to send an ack, but we can't overwrite the existing packet.
					//XXX TODO: Fixme.
					ctx->TCPs[0].seq_num = t->seq_num;//t->seq_num;
					ctx->TCPs[0].ack_num = t->ack_num;
					ctx->TCPs[0].sendtype = ACKBIT;
					rsck = 0;
					goto send_early;
				}
				else
				{
					t->sendtype = ACKBIT;
					goto send_early;
				}
			}
			else
			{
				//Do nothing
			}
		}
		else
		{
			//Otherwise, discard packet.
		}
	}

	cnip_hal_finish_callback_now( hal );

	if( did_ack_ok && rsck )
	{
		cnip_received_tcp_ack_cb( t, rsck );
	}

	return;

reset_conn0:
	rsck = 0;
	//XXX TODO: Do something with shorthand to tcps[0]
	ctx->TCPs[0].ack_num = sequence_num;
	ctx->TCPs[0].seq_num = ack_num;
	ctx->TCPs[0].sendtype = RSTBIT | FINBIT | ACKBIT;
	ctx->TCPs[0].state = 0;

send_early: //This requires a RST to be sent back.
	cnip_hal_finish_callback_now( hal );

	if( rsck == 0)
	{
		ctx->TCPs[0].dest_addr = ctx->ipsource;
		ctx->TCPs[0].dest_port = ctx->remoteport;
		ctx->TCPs[0].this_port = ctx->localport;
	}
	write_tcp_header( &ctx->TCPs[rsck] );
end_and_emit:
	cnip_tcp_end_write( &ctx->TCPs[rsck] );

	if( did_ack_ok && rsck )
	{
		cnip_received_tcp_ack_cb( t, rsck );
	}
}

int8_t CNIP_IRAM cnip_tcp_get_free_connection( cnip_ctx * ctx )
{
	uint8_t i;
	for( i = 1; i < TCP_SOCKETS; i++ )
	{
		cnip_tcp * t = &ctx->TCPs[i];
		if( !t->state )
		{
			memset( t, 0, sizeof( cnip_tcp ) );
			t->ctx = ctx;
			return i;
		}
	}
	return 0;
}

void CNIP_IRAM cnip_tcp_tick( cnip_ctx * ctx )
{
	uint8_t i;
	for( i = 0; i < TCP_SOCKETS; i++ )
	{
		cnip_tcp * t = &ctx->TCPs[i];

		if( !t->state )
			continue;

		t->idletime++;

		//XXX: TODO: Handle timeout

		if( !t->time_since_sent )
			continue;

		t->time_since_sent++;

		if( t->time_since_sent > TCP_TICKS_BEFORE_RESEND )
		{
			if( t->state == CLOSING_WAIT )
			{
				MARK( "__Closeout %d\n", i );
				t->state = 0;
				continue;
			}
			cnip_hal_xmitpacket( ctx->hal, t->sendptr, t->sendlength );

			t->time_since_sent = 1;
			if( t->retries++ > TCP_MAX_RETRIES )
			{
				MARK( "__rexmit %d\n", i );
				cnip_tcp_connection_closing_cb( t, i );
				t->state = 0;
			}
		}
	}
}

//XXX TODO This needs to be done better.
void CNIP_IRAM cnip_tcp_request_closure( cnip_tcp * t )
{
	if( !t->state ||t->state == CLOSING_WAIT ) return;
	t->sendtype = FINBIT|ACKBIT;//RSTBIT;
	t->state = CLOSING_WAIT;
	t->time_since_sent = 0;

	MARK("__req_closure %p\n", t);
//	TCPs[c].seq_num--; //???
	cnip_tcp_start_write( t );
	cnip_tcp_end_write( t );	
}


uint8_t CNIP_IRAM cnip_tcp_can_send( cnip_tcp * tcp )
{
	return !tcp->time_since_sent;
}

void CNIP_IRAM cnip_tcp_start_write( cnip_tcp * tcp )
{
	write_tcp_header( tcp );
}

static void CNIP_IRAM FinishTCPPacket( cnip_tcp * t, uint16_t length );

void CNIP_IRAM cnip_tcp_end_write(  cnip_tcp * tcp )
{
	cnip_tcp * t = tcp;
	cnip_ctx * ctx = t->ctx;
	cnip_hal * hal = ctx->hal;
	unsigned short length;
//	unsigned short payloadlen;
//	uint8_t * base = t->sendptr;

	cnip_hal_stopop( hal );

	length = cnip_hal_get_write_length( hal );
	t->sendlength = length;
	//printf( "(TCP END WRITE) Sending length %d\n", length );

	if( t->sendtype & ( PSHBIT | SYNBIT | FINBIT ) ) //PSH, SYN, RST or FIN packets
	{
		t->time_since_sent = 1;
		t->retries = 0;
	}

	//Write length in IP header

	FinishTCPPacket( t, length-20 );

	cnip_hal_endsend( hal );

#ifdef TCP_DOUBLESEND
	uint16_t nps = cnip_hal_get_scratchpad( hal );
	cnip_hal_copy_memory( nps, t->sendptr, 55, 0, 65535 );
	cnip_hal_startsend( hal, nps );
	cnip_hal_stopop( hal );
	cnip_hal_alter_word( hal, 18+6, 0x0000 );
	FinishTCPPacket( hal,40 );
	cnip_hal_xmitpacket( nps, 55 );
#endif

}

static void CNIP_IRAM FinishTCPPacket( cnip_tcp * t, uint16_t length )
{
	cnip_ctx * ctx = t->ctx;
	cnip_hal * hal = ctx->hal;

	cnip_hal_alter_word( hal, 10+6, length+6 );

#ifndef AUTO_CHECKSUMS
	cnip_hal_alter_word( hal, 18+6, cnip_hal_checksum( hal, 8+6, 20 ) );

	//Calc TCP checksum
	//Initially, write pseudo-header into the checksum area
	cnip_hal_alter_word( hal, 0x2C+6, TCP_PROTOCOL_NUMBER + length - 20 + 6 );

	cnip_hal_alter_word( hal, 0x2C+6, cnip_hal_checksum( hal, 20+6, length - 12 + 6) );
#endif

}

/*
void EmitTCP( cnip_tcp * t )
{
	cnip_hal_xmitpacket( t->sendptr, t->sendlength );
}
*/

#endif
