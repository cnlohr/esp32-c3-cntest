#ifndef _CNIP_CONFIG
#define _CNIP_CONFIG

#include <stdint.h>
#include "esp_attr.h"
#include "esp_wifi_types.h"

//#define ETH_DEBUG
#define sendstr printf

#define COMPAT_ESP32 1

#define CNIP_HAL_SCRATCHPAD_TX_SIZE 1600
#define ENABLE_DHCP_CLIENT
#define ENABLE_DHCP_SERVER
#define DHCP_SERVER_LEASE_TIME_SECONDS 86400
#define DHCP_SERVER_MAX_CLIENTS ESP_WIFI_MAX_CONN_NUM

#define CNIP_LITTLE_ENDIAN

#define INCLUDE_UDP
#define PSTR(x) x //This system does not have special rules for flash access.

//If we want to be able to send unsolicited packets...
//#define ARP_CLIENT_SUPPORT

//#define ICMP_PROFILE_CHECKSUMS //Makes pings slow, but helps profile checksum functionality.
//#ifdef ICMP_TEST_ODD_PACKETS   //Do weird stuff with word alignment.


#define INCLUDE_TCP
#define TCP_SOCKETS 20
#define TCP_TICKS_BEFORE_RESEND 15
#define TCP_BUFFERSIZE CNIP_HAL_SCRATCHPAD_TX_SIZE
#define TCP_MAX_RETRIES 15
#define CNIP_IRAM IRAM_ATTR	//Used in critical path locations.

//#define HARDWARE_HANDLES_IP_CHECKSUM

//Only valid for xtensa
//#define GetCycleCount() ({ uint32_t ret; __asm__ __volatile__("rsr %0,ccount":"=a"(ret)); ret; })
//For RV32, it's RV_READ_CSR(CSR_PCCR_MACHINE)
#define GetCycleCount cpu_ll_get_cycle_count

extern volatile uint32_t  packet_stopwatch;

typedef uint8_t * cnip_mem_address;

#endif


