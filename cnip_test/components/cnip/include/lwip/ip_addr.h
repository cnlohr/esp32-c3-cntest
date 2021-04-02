//avrcraft compatibility layer
//Borrowed heavily from lwip's ip_addr.h

#ifndef _IP_ADDR_H
#define _IP_ADDR_H

#include <stdint.h>

struct ip4_addr {
  uint32_t addr;
};

typedef struct ip4_addr ip4_addr_t;
typedef ip4_addr_t ip_addr_t;


#define ip4_addr_isany_val(addr1)   ((addr1).addr == IPADDR_ANY)
#define ip4_addr_isany(addr1) ((addr1) == NULL || ip4_addr_isany_val(*(addr1)))

#define ip4_addr_ismulticast(addr1) (((addr1)->addr & PP_HTONL(0xf0000000UL)) == PP_HTONL(0xe0000000UL))

#define ip4_addr_islinklocal(addr1) (((addr1)->addr & PP_HTONL(0xffff0000UL)) == PP_HTONL(0xa9fe0000UL))

#define ip4_addr_debug_print_parts(debug, a, b, c, d) \
  LWIP_DEBUGF(debug, ("%" U16_F ".%" U16_F ".%" U16_F ".%" U16_F, a, b, c, d))
#define ip4_addr_debug_print(debug, ipaddr) \
  ip4_addr_debug_print_parts(debug, \
                      (uint16_t)((ipaddr) != NULL ? ip4_addr1_16(ipaddr) : 0),       \
                      (uint16_t)((ipaddr) != NULL ? ip4_addr2_16(ipaddr) : 0),       \
                      (uint16_t)((ipaddr) != NULL ? ip4_addr3_16(ipaddr) : 0),       \
                      (uint16_t)((ipaddr) != NULL ? ip4_addr4_16(ipaddr) : 0))
#define ip4_addr_debug_print_val(debug, ipaddr) \
  ip4_addr_debug_print_parts(debug, \
                      ip4_addr1_16(&(ipaddr)),       \
                      ip4_addr2_16(&(ipaddr)),       \
                      ip4_addr3_16(&(ipaddr)),       \
                      ip4_addr4_16(&(ipaddr)))

/* Get one byte from the 4-byte address */
#define ip4_addr1(ipaddr) (((const uint8_t*)(&(ipaddr)->addr))[0])
#define ip4_addr2(ipaddr) (((const uint8_t*)(&(ipaddr)->addr))[1])
#define ip4_addr3(ipaddr) (((const uint8_t*)(&(ipaddr)->addr))[2])
#define ip4_addr4(ipaddr) (((const uint8_t*)(&(ipaddr)->addr))[3])
/* These are cast to uint16_t, with the intent that they are often arguments
 * to printf using the U16_F format from cc.h. */
#define ip4_addr1_16(ipaddr) ((uint16_t)ip4_addr1(ipaddr))
#define ip4_addr2_16(ipaddr) ((uint16_t)ip4_addr2(ipaddr))
#define ip4_addr3_16(ipaddr) ((uint16_t)ip4_addr3(ipaddr))
#define ip4_addr4_16(ipaddr) ((uint16_t)ip4_addr4(ipaddr))


/** 255.255.255.255 */
#define IPADDR_NONE         ((uint32_t)0xffffffffUL)
/** 127.0.0.1 */
#define IPADDR_LOOPBACK     ((uint32_t)0x7f000001UL)
/** 0.0.0.0 */
#define IPADDR_ANY          ((uint32_t)0x00000000UL)
/** 255.255.255.255 */
#define IPADDR_BROADCAST    ((uint32_t)0xffffffffUL)


uint32_t lwip_htonl(uint32_t x);

#endif

