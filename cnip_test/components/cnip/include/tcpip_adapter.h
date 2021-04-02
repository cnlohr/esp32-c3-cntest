#ifndef _TCPIP_ADAPTER_H
#define _TCPIP_ADAPTER_H

#include <lwip/ip_addr.h>
#include <lwip/ip6_addr.h>
#include <esp_wifi_types.h>

#if 0
#define IP2STR(ipaddr) ip4_addr1_16(ipaddr), \
    ip4_addr2_16(ipaddr), \
    ip4_addr3_16(ipaddr), \
    ip4_addr4_16(ipaddr)

#define IPSTR "%d.%d.%d.%d"

#define IPV62STR(ipaddr) IP6_ADDR_BLOCK1(&(ipaddr)),     \
    IP6_ADDR_BLOCK2(&(ipaddr)),     \
    IP6_ADDR_BLOCK3(&(ipaddr)),     \
    IP6_ADDR_BLOCK4(&(ipaddr)),     \
    IP6_ADDR_BLOCK5(&(ipaddr)),     \
    IP6_ADDR_BLOCK6(&(ipaddr)),     \
    IP6_ADDR_BLOCK7(&(ipaddr)),     \
    IP6_ADDR_BLOCK8(&(ipaddr))

#define IPV6STR "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"
#endif

/** @brief IPV4 IP address information
 */
typedef struct {
    ip4_addr_t ip;      /**< Interface IPV4 address */
    ip4_addr_t netmask; /**< Interface IPV4 netmask */
    ip4_addr_t gw;      /**< Interface IPV4 gateway address */
} tcpip_adapter_ip_info_t;

/** @brief IPV6 IP address information
 */
typedef struct {
    ip6_addr_t ip; /**< Interface IPV6 address */
} tcpip_adapter_ip6_info_t;

/** @brief IP address info of station connected to WLAN AP
 *
 * @note See also wifi_sta_info_t (MAC layer information only)
 */
typedef struct {
    uint8_t mac[6]; /**< Station MAC address */
    ip4_addr_t ip;  /**< Station assigned IP address */
} tcpip_adapter_sta_info_t;

/** @brief WLAN AP: Connected stations list
 *
 * Used to retrieve IP address information about connected stations.
 */
typedef struct {
    tcpip_adapter_sta_info_t sta[ESP_WIFI_MAX_CONN_NUM]; /**< Connected stations */
    int num; /**< Number of connected stations */
} tcpip_adapter_sta_list_t;







/* @brief On-chip network interfaces */
typedef enum {
    TCPIP_ADAPTER_IF_STA = 0,     /**< Wi-Fi STA (station) interface */
    TCPIP_ADAPTER_IF_AP,          /**< Wi-Fi soft-AP interface */
    TCPIP_ADAPTER_IF_ETH,         /**< Ethernet interface */
    TCPIP_ADAPTER_IF_MAX
} tcpip_adapter_if_t;

/** @brief Type of DNS server */
typedef enum {
    TCPIP_ADAPTER_DNS_MAIN= 0,       /**< DNS main server address*/
    TCPIP_ADAPTER_DNS_BACKUP,        /**< DNS backup server address (Wi-Fi STA and Ethernet only) */
    TCPIP_ADAPTER_DNS_FALLBACK,      /**< DNS fallback server address (Wi-Fi STA and Ethernet only) */
    TCPIP_ADAPTER_DNS_MAX
} tcpip_adapter_dns_type_t;

/** @brief DNS server info */
typedef struct {
    ip_addr_t ip; /**< IPV4 address of DNS server */
} tcpip_adapter_dns_info_t;

/** @brief Status of DHCP client or DHCP server */
typedef enum {
    TCPIP_ADAPTER_DHCP_INIT = 0,    /**< DHCP client/server is in initial state (not yet started) */
    TCPIP_ADAPTER_DHCP_STARTED,     /**< DHCP client/server has been started */
    TCPIP_ADAPTER_DHCP_STOPPED,     /**< DHCP client/server has been stopped */
    TCPIP_ADAPTER_DHCP_STATUS_MAX
} tcpip_adapter_dhcp_status_t;

/** @brief Mode for DHCP client or DHCP server option functions */
typedef enum{
    TCPIP_ADAPTER_OP_START = 0,
    TCPIP_ADAPTER_OP_SET,           /**< Set option */
    TCPIP_ADAPTER_OP_GET,           /**< Get option */
    TCPIP_ADAPTER_OP_MAX
} tcpip_adapter_dhcp_option_mode_t;

/* Deprecated name for tcpip_adapter_dhcp_option_mode_t, to remove after ESP-IDF V4.0 */
typedef tcpip_adapter_dhcp_option_mode_t tcpip_adapter_option_mode_t;

/** @brief Supported options for DHCP client or DHCP server */
typedef enum{
    TCPIP_ADAPTER_DOMAIN_NAME_SERVER            = 6,    /**< Domain name server */
    TCPIP_ADAPTER_ROUTER_SOLICITATION_ADDRESS   = 32,   /**< Solicitation router address */
    TCPIP_ADAPTER_REQUESTED_IP_ADDRESS          = 50,   /**< Request specific IP address */
    TCPIP_ADAPTER_IP_ADDRESS_LEASE_TIME         = 51,   /**< Request IP address lease time */
    TCPIP_ADAPTER_IP_REQUEST_RETRY_TIME         = 52,   /**< Request IP address retry counter */
} tcpip_adapter_dhcp_option_id_t;

/* Deprecated name for tcpip_adapter_dhcp_option_id_t, to remove after ESP-IDF V4.0 */
typedef tcpip_adapter_dhcp_option_id_t tcpip_adapter_option_id_t;



void tcpip_adapter_init(void);
esp_err_t tcpip_adapter_eth_start(uint8_t *mac, tcpip_adapter_ip_info_t *ip_info);
esp_err_t tcpip_adapter_sta_start(uint8_t *mac, tcpip_adapter_ip_info_t *ip_info);
esp_err_t tcpip_adapter_ap_start(uint8_t *mac, tcpip_adapter_ip_info_t *ip_info);
esp_err_t tcpip_adapter_stop(tcpip_adapter_if_t tcpip_if);
esp_err_t tcpip_adapter_up(tcpip_adapter_if_t tcpip_if);
esp_err_t tcpip_adapter_down(tcpip_adapter_if_t tcpip_if);
esp_err_t tcpip_adapter_get_ip_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_ip_info_t *ip_info);
esp_err_t tcpip_adapter_set_ip_info(tcpip_adapter_if_t tcpip_if, const tcpip_adapter_ip_info_t *ip_info);
esp_err_t tcpip_adapter_set_dns_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_dns_type_t type, tcpip_adapter_dns_info_t *dns);
esp_err_t tcpip_adapter_get_dns_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_dns_type_t type, tcpip_adapter_dns_info_t *dns);
esp_err_t tcpip_adapter_get_old_ip_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_ip_info_t *ip_info);
esp_err_t tcpip_adapter_set_old_ip_info(tcpip_adapter_if_t tcpip_if, const tcpip_adapter_ip_info_t *ip_info);
esp_err_t tcpip_adapter_create_ip6_linklocal(tcpip_adapter_if_t tcpip_if);
esp_err_t tcpip_adapter_get_ip6_linklocal(tcpip_adapter_if_t tcpip_if, ip6_addr_t *if_ip6);
esp_err_t tcpip_adapter_dhcps_get_status(tcpip_adapter_if_t tcpip_if, tcpip_adapter_dhcp_status_t *status);
esp_err_t tcpip_adapter_dhcps_option(tcpip_adapter_dhcp_option_mode_t opt_op, tcpip_adapter_dhcp_option_id_t opt_id, void *opt_val, uint32_t opt_len);
esp_err_t tcpip_adapter_dhcps_start(tcpip_adapter_if_t tcpip_if);
esp_err_t tcpip_adapter_dhcps_stop(tcpip_adapter_if_t tcpip_if);
esp_err_t tcpip_adapter_dhcpc_get_status(tcpip_adapter_if_t tcpip_if, tcpip_adapter_dhcp_status_t *status);
esp_err_t tcpip_adapter_dhcpc_option(tcpip_adapter_dhcp_option_mode_t opt_op, tcpip_adapter_dhcp_option_id_t opt_id, void *opt_val, uint32_t opt_len);
esp_err_t tcpip_adapter_dhcpc_start(tcpip_adapter_if_t tcpip_if);
esp_err_t tcpip_adapter_dhcpc_stop(tcpip_adapter_if_t tcpip_if);
esp_err_t tcpip_adapter_eth_input(void *buffer, uint16_t len, void *eb);
esp_err_t tcpip_adapter_sta_input(void *buffer, uint16_t len, void *eb);
esp_err_t tcpip_adapter_ap_input(void *buffer, uint16_t len, void *eb);
esp_interface_t tcpip_adapter_get_esp_if(void *dev);
esp_err_t tcpip_adapter_get_sta_list(const wifi_sta_list_t *wifi_sta_list, tcpip_adapter_sta_list_t *tcpip_sta_list);

#define TCPIP_HOSTNAME_MAX_SIZE    32
esp_err_t tcpip_adapter_set_hostname(tcpip_adapter_if_t tcpip_if, const char *hostname);
esp_err_t tcpip_adapter_get_hostname(tcpip_adapter_if_t tcpip_if, const char **hostname);
esp_err_t tcpip_adapter_get_netif(tcpip_adapter_if_t tcpip_if, void ** netif);
bool tcpip_adapter_is_netif_up(tcpip_adapter_if_t tcpip_if);

//bool dhcp_search_ip_on_mac(unsigned char * mac, ip4_addr_t *ip);

#endif


