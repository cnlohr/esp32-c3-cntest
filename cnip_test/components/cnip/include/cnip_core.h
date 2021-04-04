//Copyright 2012, 2019 Charles Lohr under the MIT/x11, newBSD, LGPL or GPL licenses.  You choose.

#ifndef _IPARPETC_H
#define _IPARPETC_H

#include <cnip_config.h>
#include <cnip_hal.h>
#include <stdint.h>

#ifdef INCLUDE_TCP
#include <cnip_tcp.h>
#endif

#define IP_HEADER_LENGTH 20

#define CNIPIP( a, b, c, d ) ( (a) | ( (b) <<8 ) | ( (c) << 16 ) | ( (d) << 24 ) )

#ifdef CNIP_LITTLE_ENDIAN
#define CNIP_HTONS( x )  ( ( ( x ) >> 8 ) | ( ( ( x ) & 0xff ) << 8 ) )
#define CNIP_HTONL( x )  ( ( ( x ) >> 24 ) | ( ( ( x ) & 0xff0000L ) >> 8 ) | ( ( ( x ) & 0xff00L ) << 8L ) | ( ( ( x ) & 0xffL ) << 24L ) )
#else
#define CNIP_HTONS( x ) x
#define CNIP_HTONL( x ) x
#endif

typedef uint32_t cnip_addy;


struct cnip_dhcpc_t;
struct cnip_dhcps_t;

struct cnip_ctx_t
{
	//These three must be kept IN THIS ORDER.
	//Code elsewhere relies on the ORDERING of these three!
	cnip_addy ip_addr;
	cnip_addy ip_mask;
	cnip_addy ip_gateway;

	//For a given incoming packet
	unsigned char macfrom[6];
	cnip_addy ipsource;
	unsigned short remoteport;
	unsigned short localport;

	//For bookkeeping
	unsigned long icmp_in;
	unsigned long icmp_out;

#ifdef ENABLE_DHCP_CLIENT
	struct cnip_dhcpc_t * dhcpc;
#endif

#ifdef ENABLE_DHCP_SERVER
	struct cnip_dhcps_t * dhcps;
#endif

#if defined( ENABLE_DHCP_SERVER ) || defined( ENABLE_DHCP_CLIENT )
	const char * dhcp_name;
#endif

#ifdef ARP_CLIENT_SUPPORT
	//This is not complete.
	struct ARPEntry
	{
		uint8_t mac[6];
		uint8_t ip[4];
	};

	extern uint8_t ClientArpTablePointer;
	extern struct ARPEntry ClientArpTable[ARP_CLIENT_TABLE_SIZE];
#endif

#ifdef PING_CLIENT_SUPPORT
	//This is not complete.
	struct PINGEntries
	{
		cnip_addy ip;
		uint8_t id;  //If zero, not in use.
		uint16_t last_send_seqnum;
		uint16_t last_recv_seqnum;
	};

	extern struct PINGEntries ClientPingEntries[PING_RESPONSES_SIZE];
#endif

#ifdef INCLUDE_TCP
	//All sockets (including 0) are valid.
	cnip_tcp * TCPs; //[TCP_SOCKETS];
#endif

	cnip_hal * hal;
};

typedef struct cnip_ctx_t cnip_ctx;

void cnip_init_ip( cnip_ctx * ctx );

//Utility out
void cnip_switch_to_broadcast( cnip_ctx * ctx );
void cnip_send_etherlink_header( cnip_ctx * ctx, unsigned short type );
void cnip_send_ip_header( cnip_ctx * ctx, unsigned short totallen, uint32_t to_ip, unsigned char proto );
void cnip_finish_udp_packet( cnip_ctx * ctx );
void cnip_util_emit_udp_packet( cnip_ctx * ctx, uint8_t * payload, uint32_t payloadlength, uint32_t dstip, uint16_t srcport, uint16_t dstport );

#ifdef INCLUDE_TCP
void cnip_handle_tcp( cnip_ctx * ctx, uint16_t iptotallen );
#endif

//Applications must write this.
void cnip_handle_udp( cnip_ctx * ctx, uint16_t len );

#ifdef ARP_CLIENT_SUPPORT

//Returns -1 if arp not found yet.
//Otherwise returns entry into ARP table.

int8_t cnip_request_arp( cnip_ctx * ctx, uint8_t * ip );


#endif

#ifdef PING_CLIENT_SUPPORT

#ifndef ARP_CLIENT_SUPPORT
#error Client pinging requires ARP Client.
#endif

int8_t cnip_get_ping_slot( cnip_ctx * ctx, cnip_addy ip );
void cnip_do_ping( cnip_ctx * ctx, uint8_t pingslot );

#endif


#endif

