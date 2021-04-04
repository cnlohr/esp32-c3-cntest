//Copyright 2012, 2019 Charles Lohr under the MIT/x11, newBSD, LGPL or GPL licenses.  You choose.

#include <stdio.h>
#include <cnip_core.h>
#include <alloca.h>
#include <string.h>

#define POP cnip_hal_pop8( hal )
#define POP16 cnip_hal_pop16( hal )
#define POP32LE cnip_hal_pop32LE( hal )
#define POP32BE cnip_hal_pop32BE( hal )
#define POPB(x,s) cnip_hal_popblob( hal,x,s )

#define PUSH(x) cnip_hal_push8( hal, x)
#define PUSH16(x) cnip_hal_push16( hal, x)
#define PUSH32LE(x) cnip_hal_push32LE( hal, x)
#define PUSH32BE(x) cnip_hal_push32BE( hal, x)
#define PUSHB(x,s) cnip_hal_pushblob( hal, x,s)

#ifdef INCLUDE_TCP
#include <cnip_tcp.h>
#endif

#ifdef CNIP_MDNS
#include <cnip_mdns.h>
#endif

#if defined( ENABLE_DHCP_CLIENT ) || defined( ENABLE_DHCP_SERVER ) 
#include <cnip_dhcp.h>
#endif


#if defined( ENABLE_CNIP_TFTP_SERVER ) 
#include <cnip_tftp.h>
#endif


//It would be really good to switch away from the POP and PUSH mentality.
//This IP stack is NOT for embedded devices with an offboard MAC.

struct etherlink_header
{
	uint8_t dest_mac[6];
	uint8_t from_mac[6];
	uint8_t hwtype;
	uint8_t pktype;
	//After here would be ARP or normal land, or IP land.  Do not add to this structure otherwise you will mess up ARP.
} __attribute__((packed));

struct ip_header
{
	uint8_t ip_fmt;
	uint8_t differentiated_services; //Not used.
	uint16_t total_len;
	uint16_t identification; //unused
	uint16_t frag_offset_and_flags;
	uint8_t  ttl;
	uint8_t  proto;
	uint16_t checksum;
	uint32_t src_ip;
	uint32_t dest_ip;
} __attribute__((packed));


void cnip_init_ip( cnip_ctx * ctx )
{
	memset( &ctx->ip_addr, 0, 12 ); //Zero out all 3 addresses.

#ifdef INCLUDE_TCP
	cnip_tcp_init( ctx );
#endif
}

void CNIP_IRAM cnip_send_etherlink_header( cnip_ctx * ctx, unsigned short type )
{
	struct etherlink_header * hdr = (struct etherlink_header*)cnip_hal_push_ptr_and_advance( ctx->hal, 14 );
	memcpy( hdr->dest_mac, ctx->macfrom, 6 );
	memcpy( hdr->from_mac, ctx->hal->mac, 6 );  //From
	hdr->hwtype = type >> 8;
	hdr->pktype = type & 0xff;
}

void CNIP_IRAM cnip_send_ip_header( cnip_ctx * ctx, unsigned short totallen, uint32_t to_ip, unsigned char proto )
{
	struct ip_header * iph = (struct ip_header*)cnip_hal_push_ptr_and_advance( ctx->hal, sizeof( struct ip_header ) );
	iph->ip_fmt = 0x45;
	iph->differentiated_services = 0;
	iph->total_len = CNIP_HTONS( totallen );
	iph->identification = 0;
	iph->frag_offset_and_flags = 0x0040;
	iph->ttl = 64;
	iph->proto = proto;
	iph->checksum = 0;
	iph->src_ip = ctx->ip_addr;
	iph->dest_ip = to_ip;
}


static void CNIP_IRAM HandleICMP( cnip_ctx * ctx, int totallen )
{
	cnip_hal * hal = ctx->hal;
	struct icmp_header_base
	{
		uint8_t type;
		uint8_t code;
		uint16_t checksum;
		uint16_t id;
	} * icmph = (struct icmp_header_base*)cnip_hal_pop_ptr_and_advance( hal, 6 );

	ctx->icmp_in++;

	switch( icmph->type )
	{
#ifdef PING_CLIENT_SUPPORT
	case 0: //Response
{
		uint16_t id = icmph->id;

		//Should probably do other checks here, this is awful and can have collisions.
		if( id < PING_RESPONSES_SIZE )
		{
			ClientPingEntries[id].last_recv_seqnum = POP16; //Seqnum
		}
		cnip_hal_stopop( hal );

		cnip_hal_finish_callback_now( hal );
}
		break;
#endif

	case 8: //ping request
		//This is particularly tricky since we are operating inside the
		//the incoming buffer.
		{
			short payloadsize = cnip_hal_bytesleft(hal); //includes identifier + seqno + payload
			payloadsize = payloadsize; //In case it's not used.

#ifdef ICMP_TEST_ODD_PACKETS
			static int offset;
			cnip_hal_startsend( hal, cnip_hal_get_scratchpad( hal )+((offset+=2)&0xf), CNIP_HAL_SCRATCHPAD_TX_SIZE-4 );
			memcpy( hal->outgoing_base, hal->incoming_base, payloadsize + 42 );
#else
			cnip_hal_make_output_from_input( hal );	
#endif

			cnip_send_etherlink_header( ctx, 0x0800 );
			cnip_send_ip_header( ctx, totallen, ctx->ipsource, 0x01 );
#ifdef ICMP_PROFILE_CHECKSUMS
			PUSH32LE( 0 ); //ping reply + code Zero the initial CRC.
#else
			PUSH16( 0 ); //ping reply + DO NOT code Zero the initial CRC.
#endif
			cnip_force_packet_length( hal, totallen + 14 );	//Basically shoe in the rest of the packet.
#ifndef HARDWARE_HANDLES_IP_CHECKSUM
			cnip_hal_alter_word( hal, 18+6, cnip_hal_checksum( hal, 8+6, 20 ) );
#endif

			//This is a wee bit weird, but we know what the checksum will be, based on the initial checksum, if we just
			//trust it started uncorrupted.
#ifndef ICMP_PROFILE_CHECKSUMS
			uint32_t csum = (((uint8_t*)hal->outgoing_base)[30+6] << 8 ) | ((uint8_t*)hal->outgoing_base)[30+6+1];
			csum += 0x800; //Switching from request to reply.
			if( csum >> 16 )
			{
				csum = (csum & 0xffff) + (csum >> 16);
			}
			((uint8_t*)hal->outgoing_base)[30+6] = csum >> 8;
			((uint8_t*)hal->outgoing_base)[30+6+1] = csum & 0xff;
#else
			PUSH16( 0 ); //Checksum standin
			cnip_hal_alter_word( hal, 30+6, cnip_hal_checksum( hal, 28+6, payloadsize + 6 ) );
#endif
			cnip_hal_endsend( hal );
		}

		ctx->icmp_out++;

		break;
	}

}


static void HandleArp( cnip_ctx * ctx )
{
	cnip_hal * hal = ctx->hal;
	unsigned char sendermac_ip_and_targetmac[16];
//	unsigned char senderip[10]; //Actually sender ip + target mac, put in one to shrink code.

	unsigned short proto;
	unsigned char opcode;
//	unsigned char ipproto;

	cnip_hal_dumpbytes( hal, 2 ); //Hardware type
	proto = POP16;
	cnip_hal_dumpbytes( hal, 2 ); //hwsize, protosize
	opcode = POP16;  //XXX: This includes "code" as well, it seems.

	switch( opcode )
	{
	case 1:	//Request
{
		unsigned char match;

		POPB( sendermac_ip_and_targetmac, 16 );

		match = 1;
//sendhex2( 0xff );

		//Target IP (check for copy)
		cnip_addy popip = POP32LE;
		if( popip != ctx->ip_addr ) match = 0;

		if( match == 0 )
			return;

		//We must send a response, so we termiante the packet now.
		cnip_hal_finish_callback_now( hal );
		cnip_hal_startsend( hal, cnip_hal_get_scratchpad( hal ), CNIP_HAL_SCRATCHPAD_TX_SIZE );
		cnip_send_etherlink_header( ctx, 0x0806 );

		PUSH32BE( 0x00010000 | proto ); //Ethernet + proto
		PUSH32BE( 0x06040002 ); //HW size, Proto size || reply

		PUSHB( hal->mac, 6 );
		PUSH32LE( ctx->ip_addr );
		PUSHB( sendermac_ip_and_targetmac, 10 ); // do not send target mac. 2019 charles here: this comment is confusing.
		cnip_hal_endsend( hal );

//		sendstr( "have match!\n" );

		break;
}
#ifdef ARP_CLIENT_SUPPORT
	case 2: //ARP Reply
{
		uint8_t sender_mac_and_ip_and_comp_mac[16];
		POPB( sender_mac_and_ip_and_comp_mac, 16 );
		cnip_hal_finish_callback_now( hal );


		//First, make sure that we're the ones who are supposed to receive the ARP.
		for( i = 0; i < 6; i++ )
		{
			if( sender_mac_and_ip_and_comp_mac[i+10] != MyMAC[i] )
				break;
		}

		if( i != 6 )
			break;

		//We're the right recipent.  Put it in the table.
		memcpy( &ClientArpTable[ClientArpTablePointer], sender_mac_and_ip_and_comp_mac, 10 );

		ClientArpTablePointer = (ClientArpTablePointer+1)%ARP_CLIENT_TABLE_SIZE;
}
#endif

	default:
		//???? don't know what to do.
		return;
	}
}



void CNIP_IRAM __attribute__((weak)) cnip_handle_udp( cnip_ctx * ctx, uint16_t len )
{
}

void CNIP_IRAM cnip_hal_receivecallback( cnip_hal * hal )
{
	cnip_ctx * ctx = hal->ip_ctx;

	{
		struct etherlink_header * pkt = (struct etherlink_header*)cnip_hal_pop_ptr_and_advance( hal, sizeof( struct etherlink_header ) );
		//First and foremost, make sure we have a big enough packet to work with.
		if( !pkt )
		{
	#ifdef ETH_DEBUG
			sendstr( "Runt\n" );
	#endif
			return;
		}

		//macto (ignore) our mac filter handles this.
		//cnip_hal_dumpbytes( hal, 6 );
		//POPB( , 6 );
		memcpy( ctx->macfrom, pkt->from_mac, 6 );
		//Make sure it's ethernet!
		if( pkt->hwtype != 0x08 )
		{
	#ifdef ETH_DEBUG
			sendstr( "Not ethernet.\n" );
	#endif
			return;
		}

		//Is it ARP?
		if( pkt->pktype == 0x06 )
		{
			HandleArp( ctx );
			return;
		}
	}

	struct ip_header * pkt = (struct ip_header*)cnip_hal_pop_ptr_and_advance( hal, sizeof( struct ip_header ) );

	if( !pkt )
	{
		//Unknown packet.  It's not IP, so we don't care about it.
		return;
	}
	//otherwise it's standard IP

	//So, we're expecting a '45
	if( pkt->ip_fmt != 0x45 )
	{
#ifdef ETH_DEBUG
		sendstr( "Not IP.\n" );
#endif
		return;
	}

	int iptotallen = CNIP_HTONS( pkt->total_len );
	iptotallen = iptotallen;
	unsigned char ipproto = pkt->proto;

	ctx->ipsource = pkt->src_ip;
	cnip_addy dest = pkt->dest_ip;

	uint8_t is_the_packet_for_me = 
		!(dest ^ ctx->ip_addr)
		  ||
		!(~(dest | ctx->ip_mask));

	//Tricky, for DHCP packets, we have to detect it even if it is not to us.
	//So this doesn't go down into the bottom section.

	if( ipproto == 17 )
	{
		//If we think it may be a DHCP packet, or even if not, we can still
		//pull off the port number, etc.
		ctx->remoteport = POP16;
		ctx->localport = POP16;

#if defined( ENABLE_DHCP_CLIENT ) || defined( ENABLE_DHCP_SERVER )
		if( ctx->localport == 68 || ctx->localport == 67 )
		{
			cnip_dhcp_handle( ctx, POP16 ); //Pop16 gets us the length.
			return;
		}
#endif
#ifdef CNIP_MDNS
		if( ctx->localport == 5353 )
		{
			got_mdns_packet(ctx, POP16 );
		}
#endif
	}

	if( !is_the_packet_for_me )
	{
#ifdef ETH_DEBUG
		sendstr( "not for me\n" );
#endif
		return;
	}


	//XXX TODO Handle IPL > 5  (IHL?)
	switch( ipproto )
	{
	case 1: //ICMP

		HandleICMP( ctx, iptotallen );
		break;
#ifdef INCLUDE_UDP
	case 17:
	{
		uint16_t length = POP16 - 8; //Minus the UDP header.
		POP16; //Checksum (assume lower layers have handled it)
		//printf( "UDP PORT: %d %d // %d\n", ctx->remoteport, ctx->localport, length );

		#if defined( ENABLE_CNIP_TFTP_SERVER ) 
		if( ctx->localport == 69 || ( ctx->localport >= TFTP_LOCAL_BASE_PORT && ctx->localport < TFTP_LOCAL_BASE_PORT+MAX_TFTP_CONNECTIONS ) )
		{
			HandleCNIPTFTPUDPPacket( ctx, length );
		}
		else
		#endif
		{
			cnip_handle_udp( ctx, length );
		}
		break;	
	}
#endif
#ifdef INCLUDE_TCP
	case 6: // TCP
	{
		ctx->remoteport = POP16;
		ctx->localport = POP16;
		iptotallen-=20;
		cnip_tcp_handle( ctx, iptotallen );
		break;
	}
#endif // HAVE_TCP_SUPPORT
	default:
		cnip_hal_finish_callback_now();
		break;
	}
}


#ifdef INCLUDE_UDP

#define CNIP_ADD_MAC 0
void cnip_finish_udp_packet( cnip_ctx * ctx )// unsigned short length )
{
	cnip_hal * hal = ctx->hal;

	//Packet loaded.
	cnip_hal_stopop( hal );
	#define CNIPSUBMAC (-6*CNIP_ADD_MAC)

	unsigned short length = cnip_hal_get_write_length( hal ) - CNIPSUBMAC; //6 = my mac
	//Write length in packet
	cnip_hal_alter_word( hal, 10+6, length-14 ); //Experimentally determined 14, 0 was used for a long time.
#ifndef HARDWARE_HANDLES_IP_CHECKSUM
	//If we were on hardware which offloaded checksums, we could be clever about pipelining the work.
	cnip_hal_alter_word( hal, 18+6, cnip_hal_checksum( hal, 8+6, 20 ) );
#endif
	cnip_hal_alter_word( hal, 32+6, length-34 );
	cnip_hal_alter_word( hal, 34+6, 0x11 + 0x8 + length-42 ); //UDP number + size + length (of packet)

	//Addenudm for UDP checksum
#ifndef HARDWARE_HANDLES_UDP_CHECKSUM
	uint16_t ppl2 = cnip_hal_checksum( hal, 20+6, length - 42 + 16 );
	if( ppl2 == 0 ) ppl2 = 0xffff;
	cnip_hal_alter_word( hal, 34+6, ppl2 );
#endif

	cnip_hal_endsend( hal );
}


void cnip_util_emit_udp_packet( cnip_ctx * ctx, uint8_t * payload, uint32_t payloadlength, uint32_t dstip, uint16_t srcport, uint16_t dstport )
{
	// Truncate packet
	if( payloadlength > CNIP_HAL_SCRATCHPAD_TX_SIZE - 40 )
	{
		payloadlength = CNIP_HAL_SCRATCHPAD_TX_SIZE - 40;
	}
	cnip_hal * hal = ctx->hal;
	cnip_hal_stopop( hal );
	cnip_hal_startsend( hal, cnip_hal_get_scratchpad( hal ), CNIP_HAL_SCRATCHPAD_TX_SIZE ); 
	cnip_send_etherlink_header( ctx, 0x0800 );
	cnip_send_ip_header( ctx, 0 /* Will get filled in */, dstip, 17 );
	cnip_hal_push16( hal, srcport );
	cnip_hal_push16( hal, dstport );
	cnip_hal_push32LE( hal, 0 ); //Will be length and checksum
	cnip_hal_pushblob( hal, payload, payloadlength );
	cnip_finish_udp_packet( ctx );
}

#endif



void CNIP_IRAM cnip_switch_to_broadcast( cnip_ctx * ctx )
{
	int i;
	//Set the address we want to send to (broadcast)
	for( i = 0; i < 6; i++ )
		ctx->macfrom[i] = 0xff;
}

#ifdef ARP_CLIENT_SUPPORT

int8_t cnip_request_arp( cnip_ctx * ctx, uint8_t * ip )
{
	cnip_hal * hal = ctx->hal;

	uint8_t i;

	for( i = 0; i < ARP_CLIENT_TABLE_SIZE; i++ )
	{
		if( memcmp( (char*)&ClientArpTable[i].ip, ip, 4 ) == 0 ) //XXX did I mess up my casting?
		{
			return i;
		}
	}

	cnip_switch_to_broadcast( ctx );

	//No MAC Found.  Send an ARP request.
	cnip_hal_finish_callback_now();
	cnip_hal_startsend( hal, cnip_hal_get_scratchpad( hal ), CNIP_HAL_SCRATCHPAD_TX_SIZE );
	cnip_send_etherlink_header( ctx, 0x0806 );

	PUSH16( 0x0001 ); //Ethernet
	PUSH16( 0x0800 ); //Protocol (IP)
	PUSH16( 0x0604 ); //HW size, Proto size
	PUSH16( 0x0001 ); //Request

	PUSHB( hal->mac, 6 );
	PUSHB( &ctx->ip_addr, 4 );
	PUSH16( 0x0000 );
	PUSH16( 0x0000 );
	PUSH16( 0x0000 );
	PUSHB( ip, 4 );

	cnip_hal_endsend( hal );

	return -1;
}

struct ARPEntry ClientArpTable[ARP_CLIENT_TABLE_SIZE];
uint8_t ClientArpTablePointer = 0;

#endif

#ifdef PING_CLIENT_SUPPORT

struct PINGEntries ClientPingEntries[PING_RESPONSES_SIZE];

int8_t cnip_get_ping_slot( cnip_ctx * ctx, cnip_addy ip )
{
	uint8_t i;
	for( i = 0; i < PING_RESPONSES_SIZE; i++ )
	{
		if( !ctx->ClientPingEntries[i].id ) break;
	}

	if( i == PING_RESPONSES_SIZE )
		return -1;

	ctx->ClientPingEntries[i].ip = ip;
	return i;
}

void cnip_do_ping( cnip_ctx * ctx, uint8_t pingslot )
{
	cnip_hal * hal = ctx->hal;
	unsigned short ppl;	
	uint16_t seqnum = ++ctx->ClientPingEntries[pingslot].last_send_seqnum;
	uint16_t checksum = (seqnum + pingslot + 0x0800) ;

	int8_t arpslot = cnip_request_arp( ctx, ClientPingEntries[pingslot].ip );

	if( arpslot < 0 ) return;

	//must set macfrom to be the IP address of the target.
	memcpy( macfrom, ctx->ClientArpTable[arpslot].mac, 6 );

	cnip_hal_startsend( ctx->hal, cnip_hal_get_scratchpad( ctx->hal ), CNIP_HAL_SCRATCHPAD_TX_SIZE );
	cnip_send_etherlink_header( ctx, 0x0800 );
	cnip_send_ip_header( ctx, 32, ClientPingEntries[pingslot].ip, 0x01 );

	PUSH16( 0x0800 ); //ping request + 0 for code
	PUSH16( ~checksum ); //Checksum
	PUSH16( pingslot ); //Idneitifer
	PUSH16( seqnum ); //Sequence number

	PUSH16( 0x0000 ); //Payload
	PUSH16( 0x0000 );

	cnip_hal_alter_word( hal, 18, cnip_hal_checksum( hal, 8, 20 ) );

	cnip_hal_endsend( hal );
}

#endif


