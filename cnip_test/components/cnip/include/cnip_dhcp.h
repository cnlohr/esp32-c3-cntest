#ifndef _DHCPC_H
#define _DHCPC_H

#include <cnip_config.h>

#ifdef ENABLE_DHCP_CLIENT

#include <cnip_core.h>

struct cnip_dhcpc_t
{
	uint8_t dhcp_clocking;
	uint16_t dhcp_seconds_remain;
	uint8_t dhcp_ticks_in_sec;
	const char * dhcp_name;
};
typedef struct cnip_dhcpc_t cnip_dhcpc;

struct cnip_dhcps_t
{
	//IP is determined by index.
	uint8_t  macs[DHCP_SERVER_MAX_CLIENTS][6];
	uint32_t ticktimeoflease[DHCP_SERVER_MAX_CLIENTS];
	uint32_t server_upticks;
};
typedef struct cnip_dhcps_t cnip_dhcps;

void cnip_dhcpc_create( struct cnip_ctx_t * ctx );
void cnip_dhcps_create( struct cnip_ctx_t * ctx );

void cnip_dhcp_send_request( struct cnip_ctx_t * ctx, uint8_t mode, cnip_addy negotiating_ip, cnip_addy remote_address, cnip_addy dest_addy, uint8_t * transaction_id );



void cnip_dhcp_handle( struct cnip_ctx_t * ctx, int udplen );


//NOTE: This cannot exceed 255
#ifndef DHCP_TICKS_PER_SECOND
#define DHCP_TICKS_PER_SECOND 10
#endif

void cnip_set_dhcp_name( struct cnip_ctx_t * ctx, const char * name  );
void cnip_tick_dhcp( struct cnip_ctx_t * ctx ); //Call this DHCP_TICKS_PER_SECOND times per second.

//If DHCP is enabled, you must write this function:
void cnip_got_dhcp_lease_cb( struct cnip_ctx_t * ctx );


#endif
#endif

