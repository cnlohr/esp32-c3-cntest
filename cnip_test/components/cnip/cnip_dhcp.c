#include <cnip_dhcp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


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

#if defined( ENABLE_DHCP_CLIENT ) || defined( ENABLE_DHCP_SERVER )


void cnip_dhcpc_create( cnip_ctx * ctx )
{
	cnip_dhcpc * dc = ctx->dhcpc = malloc( sizeof( cnip_dhcpc ) );
	dc->dhcp_clocking = 1;
	dc->dhcp_seconds_remain = 0;
	dc->dhcp_ticks_in_sec = 0;
	dc->dhcp_name = 0;

}

void cnip_dhcps_create( cnip_ctx * ctx )
{
	cnip_dhcps * ds = ctx->dhcps = malloc( sizeof( cnip_dhcps ) );
	memset( ds, 0, sizeof( *ds ) );
}
#if 0
bool dhcp_search_ip_on_mac(u8_t *mac, ip4_addr_t *ip)
{
#ifdef ENABLE_DHCP_SERVER
	if( message_type == 1 && ds )
	{
		//DHCP REQUEST
		cnip_hal_finish_callback_now();
		int i;
		int besti = 0;
		uint32_t worsttime = 0;
		for( i = 0; i < DHCP_SERVER_MAX_CLIENTS; i++ )
		{
			if( memcmp( mac, ds->macs[i], 6 ) == 0 )
			{
				ip[0] = 192;	//XXX TODO: Fixme.
				ip[1] = 168;
				ip[2] = 4;
				ip[3] = i;
				reutrn true;
			}
		}
#endif
	return false;
}
#endif

#ifndef INCLUDE_UDP
#error ERROR: You must have UDP support for DHCP support.
#endif

void cnip_dhcp_handle( cnip_ctx * ctx, int udplen )
{
#if defined( ENABLE_DHCP_CLIENT )
	cnip_dhcpc * dc = ctx->dhcpc;
#endif

#if defined( ENABLE_DHCP_SERVER )
	cnip_dhcps * ds = ctx->dhcps;
#endif

	cnip_addy client_ip;
	uint32_t transaction_id;
	uint8_t optionsleft = 48; //We only allow for up to 48 options
	uint8_t is_ack_packet = 0;
	uint16_t first4, second4;

	cnip_hal * hal = ctx->hal;

	cnip_hal_dumpbytes( hal, 2 ); //Clear out checksum

	//Process DHCP!
	uint8_t message_type = POP;
	uint8_t hardware_type = POP;
	hardware_type = hardware_type;

	cnip_hal_dumpbytes( hal, 2 ); //size of packets + Hops
	cnip_hal_popblob( hal, (uint8_t*)&transaction_id, 4 );

	cnip_hal_dumpbytes( hal, 8 ); //time elapsed + bootpflags

	client_ip = POP32LE;

	cnip_hal_dumpbytes( hal, 0x18 + 0xc0 ); //Next IP + Relay IP + Mac + Padding + server name + boot name

	uint32_t cookie = POP32BE;

	if( cookie != 0x63825363 )
	{
		return;
	}

#ifdef ENABLE_DHCP_SERVER
	if( message_type == 1 && ds )
	{
		//DHCP REQUEST
		cnip_hal_finish_callback_now();
		int i;
		int besti = 0;
		uint32_t worsttime = 0;
		for( i = 0; i < DHCP_SERVER_MAX_CLIENTS; i++ )
		{
			if( memcmp( ctx->macfrom, ds->macs[i], 6 ) == 0 )
			{
				besti = 0;
				break;
			}
			uint32_t dtime = ds->server_upticks - ds->ticktimeoflease[i];
			if( dtime > worsttime )
			{
				besti = i;
				worsttime = dtime;
			}
		}
		//besti + 2 is last octet!
		cnip_addy client_addy = ctx->ip_addr;
		((uint8_t*)&client_addy)[3] += besti+1;
		ds->ticktimeoflease[besti] = ds->server_upticks;
		memcpy( ds->macs[besti], ctx->macfrom, 6 );

//		printf( "SENDING: %08x %08x %08x\n", ctx->ip_addr, ctx->ip_addr, client_addy  );
		//Assume that if we have a client_ip, we should be acking instead.

		int send_ack = 2; //2 =  ((mode==2)?2:((mode==6)?5:6)) (THIS IS NOT THE ACTUAL RESPONSE)
		int is_req = 0;

		while( optionsleft-- )
		{
			uint8_t option = POP;
			uint8_t length = POP;
			switch(option)
			{
			case 0x35: 
			{
				if( length < 1 ) return;
				uint8_t rqt = POP;
				if( rqt == 3 ) is_req = 1;
				length--;
				break;
			}
			case 0x32: //Requested address
			{
				uint32_t reqaddy = POP32LE;
				if( is_req )
				{
					if( client_addy == reqaddy )
						send_ack = 6;
					else
						send_ack = 8;
				}
				length -= 4;
				break;
			}
			case 0xff:
				optionsleft = 0;
				break;
			default:
				break;
			}
			//Cleanup remaining bytes here.
			cnip_hal_dumpbytes( hal, length );
			//XXX TODO: Should monitor bounds on packet here.
		}

		cnip_dhcp_send_request( ctx, send_ack, ctx->ip_addr, ctx->ip_addr, client_addy, (uint8_t*)&transaction_id );  //Request
	}
	else
#endif

#if defined( ENABLE_DHCP_CLIENT )
	if( message_type  == 2 && dc )
	{
		//DHCP ACK

		//Make sure transaction IDs match.
		if( memcmp( &transaction_id, hal->mac, 4 ) != 0 )
		{
			//Not our request?
			return;
		}

		//Ok, we know we have a valid DHCP packet.

		//We dont want to get stuck, so we will eventually bail if we have an issue pasrsing.
		while( optionsleft-- )
		{
			uint8_t option = POP;
			uint8_t length = POP;

			switch(option)
			{
			case 0x35: //DHCP Message Type
			{
				if( length < 1 ) return;
				uint8_t rqt = POP;

				if( rqt == 0x02 ) 
				{
					//We have an offer, extend a request.
					//We will ignore the rest of the packet.
					cnip_hal_finish_callback_now();
					cnip_dhcp_send_request( ctx, 3, client_ip, ctx->ipsource, 0xffffffff, hal->mac );  //Request
					return;
				}
				else if( rqt == 0x05 ) // We received an ack, accept it.
				{
					//IP Is valid.
					is_ack_packet = 1;
					if( 0 == dc->dhcp_seconds_remain )
						dc->dhcp_seconds_remain = 0xffff;
					ctx->ip_addr = client_ip;
				}

				length--;
				break;
			}
			case 0x3a: //Renewal time
			{
				if( length < 4 || !is_ack_packet ) break;
				first4 = POP16;
				second4 = POP16;
	//			printf( "LEASE TIME: %d %d\n", first4, second4 );
				if( first4 )
				{
					dc->dhcp_seconds_remain = 0xffff;
				}
				else
				{
					dc->dhcp_seconds_remain = second4;
				}

				length -= 4;
				break;
			}
			case 0x01: //Subnet mask
			{
				if( length < 4 || !is_ack_packet ) break;
				cnip_hal_popblob( hal, (uint8_t*)&ctx->ip_mask, 4 );
				length -= 4;
				break;
			}
			case 0x03: //Router mask
			{
				if( length < 4 || !is_ack_packet ) break;
				cnip_hal_popblob( hal, (uint8_t*)&ctx->ip_gateway, 4 );
				length -= 4;
				break;
			}
			case 0xff:  //End of list.
				cnip_hal_finish_callback_now( hal );
				if( is_ack_packet )
					cnip_got_dhcp_lease_cb( ctx );
				return; 
			case 0x42: //Time server
			case 0x06: //DNS server
			default:
				break;
			}
			cnip_hal_dumpbytes( hal, length );
		}
	}
#endif

}

void cnip_set_dhcp_name( cnip_ctx * ctx, const char * name  )
{
	ctx->dhcp_name = name;
}

void cnip_tick_dhcp( cnip_ctx * ctx )
{
#if defined( ENABLE_DHCP_CLIENT )
	cnip_dhcpc * dc = ctx->dhcpc;
	if( dc )
	{
		if( dc->dhcp_ticks_in_sec++ < DHCP_TICKS_PER_SECOND )
		{
			return;
		}

		dc->dhcp_ticks_in_sec = 0;
	//		sendhex4( dhcp_seconds_remain );

		if( dc->dhcp_seconds_remain == 0 )
		{
			//No DHCP received yet.
			if( dc->dhcp_clocking == 250 ) dc->dhcp_clocking = 0;
			dc->dhcp_clocking++;
			cnip_dhcp_send_request( ctx, 1, ctx->ip_addr, 0, 0xffffffff, ctx->hal->mac );
		}
		else
		{
			dc->dhcp_seconds_remain--;
		}
	}
#endif

#if defined( ENABLE_DHCP_SERVER )
	cnip_dhcps * ds = ctx->dhcps;
	if( ds )
	{
		ds->server_upticks++;
	}
#endif
}


#if defined( ENABLE_DHCP_CLIENT )
//Mode = 1 for discover, Mode = 3 for request - if discover, dhcp_server should be 0. 
void cnip_dhcp_send_request( cnip_ctx * ctx, uint8_t mode, cnip_addy negotiating_ip, cnip_addy server, cnip_addy dest_addy, uint8_t * transaction_id )
{
	cnip_dhcpc * dc = ctx->dhcpc;
	cnip_dhcps * ds = ctx->dhcps;

	ds = ds;
	dc = dc;

	cnip_hal * hal = ctx->hal;
	cnip_addy oldip;

	if( mode & 1 )
	{
		cnip_switch_to_broadcast( ctx );
	}
	else
	{
	}

	cnip_hal_stopop( hal );
	cnip_hal_startsend( hal, cnip_hal_get_scratchpad( hal ), CNIP_HAL_SCRATCHPAD_TX_SIZE ); 
	cnip_send_etherlink_header( ctx, 0x0800 );

	//Tricky - backup our IP - we want to spoof it to 0.0.0.0

	if( mode & 1 )
	{
		oldip = ctx->ip_addr;
		ctx->ip_addr = 0;
	}

	cnip_send_ip_header( ctx, 0, dest_addy, 17 ); //UDP Packet to 255.255.255.255

	if( mode & 1 )
	{
		ctx->ip_addr = oldip;
	}

/*
	cnip_hal_push16( hal, 68 );  //From bootpc
	cnip_hal_push16( hal, 67 );  //To bootps
	cnip_hal_push16( hal, 0 ); //length for later
	cnip_hal_push16( hal, 0 ); //csum for later

	//Payload of BOOTP packet.
	//	1, //Bootp request
	//	1, //Ethernet
	cnip_hal_push16( 0x0101 );
	//	6, //MAC Length
	//	0, //Hops
	cnip_hal_push16( 0x0600 );
*/
	if( mode & 1 )
	{
		cnip_hal_push16( hal, 68 );  //From bootpc
		cnip_hal_push16( hal, 67 );  //To bootps
	}
	else
	{
		cnip_hal_push16( hal, 67 );  //To bootps
		cnip_hal_push16( hal, 68 );  //From bootpc
	}
	
	cnip_hal_push32LE( hal, 0 );
	int messagetype = mode;
	if( messagetype == 3 ) messagetype = 1;
	cnip_hal_push32LE( hal, messagetype | 0x00060100 );
//	cnip_hal_pushpgmblob( hal, (void*)PSTR("\x00\x00\x00\x00\x02\x01\x06"), 8 ); //NOTE: Last digit is 0 on wire, not included in string.

	cnip_hal_pushblob( hal, transaction_id, 4 );

	cnip_hal_push16( hal, dc?dc->dhcp_clocking:0 ); //seconds
	cnip_hal_pushzeroes( hal, 6 ); //unicast, CLIADDR

	if( mode & 1 )
	{
		cnip_hal_push32LE( hal, 0x00000000 );
	}
	else
	{
		cnip_hal_push32LE( hal, dest_addy );
	}

	if( server )
		cnip_hal_pushblob( hal, (uint8_t*)&server, 4 );
	else
		cnip_hal_pushzeroes( hal, 4 );

	cnip_hal_pushzeroes( hal, 4 ); //GIADDR IP

	if( mode & 1 )
	{
		cnip_hal_pushblob( hal, hal->mac, 6 ); //client mac
	}
	else
	{
		cnip_hal_pushblob( hal, ctx->macfrom, 6 ); //client mac
	}

	cnip_hal_pushzeroes( hal, 10 + 0x40 + 0x80 ); //padding + Server Host Name

	PUSH32BE( 0x63825363 );

	if( mode & 1 )
	{
		//Now we are on our DHCP portion
		cnip_hal_push8( hal, 0x35 );  //DHCP Message Type
		cnip_hal_push16( hal, 0x0100 | mode );

		//You're a client, sending to a server.
		{
			cnip_hal_push16( hal, 0x3204 ); //Requested IP address. (4 length)
			cnip_hal_pushblob( hal, (uint8_t*)&negotiating_ip, 4 );
		}

		if( server ) //request
		{
			cnip_hal_push16( hal, 0x3604 );
			cnip_hal_pushblob( hal, (uint8_t*)&server, 4 );
		}

		if( ctx->dhcp_name )
		{
			uint8_t namelen = strlen( ctx->dhcp_name );
			cnip_hal_push8( hal, 0x0c ); //Name
			cnip_hal_push8( hal, namelen );
			cnip_hal_pushblob( hal, (uint8_t*)ctx->dhcp_name, namelen );
		}

		cnip_hal_push16( hal, 0x3702 ); //Parameter request list
		cnip_hal_push16( hal, 0x0103 ); //subnet mask, router
	//	cnip_hal_push16( hal, 0x2a06 ); //NTP server, DNS server  (We don't use either NTP or DNS)
	}
	else
	{
		//Now we are on our DHCP portion
		cnip_hal_push8( hal, 0x35 );  //DHCP Message Type
		cnip_hal_push16( hal, 0x0100 | ((mode==2)?2:((mode==6)?5:6)) );

		//Lease time
		cnip_hal_push8( hal, 0x33);  //DHCP Message Type
		cnip_hal_push8( hal, 4 );
		cnip_hal_push32BE( hal, DHCP_SERVER_LEASE_TIME_SECONDS );

		cnip_hal_push8( hal, 0x01 );  //Subnet Mask
		cnip_hal_push8( hal, 4 );
		cnip_hal_push32LE( hal, ctx->ip_mask );

		cnip_hal_push8( hal, 0x36 );  //Server Identifier
		cnip_hal_push8( hal, 4 );
		cnip_hal_push32LE( hal, ctx->ip_addr );

		cnip_hal_push8( hal, 0x3 );  //Gateway
		cnip_hal_push8( hal, 4 );
		cnip_hal_push32LE( hal, ctx->ip_gateway );

	}

	cnip_hal_push8( hal, 0xff ); //End option

	cnip_hal_pushzeroes( hal, 32 ); //Padding

	cnip_finish_udp_packet( ctx );
}
#endif

#endif


