#pragma once

#include "lwip/ip_addr.h"

#define NETIF_MAX_HWADDR_LEN 6U
#define ip_addr_set_zero(ipaddr)                ip4_addr_set_zero(ipaddr)

#define ip4_addr_set_zero(ipaddr)     ((ipaddr)->addr = 0)


