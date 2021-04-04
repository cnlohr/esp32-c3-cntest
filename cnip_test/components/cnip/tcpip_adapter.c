////////////////////////////////////////////////////////////////////////////////
// THIS FILE WILL SOON BE DEPRECATED ///////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


#include <tcpip_adapter.h>
#include <cnip_hal.h>
#include <cnip_core.h>
#include <cnip_dhcp.h>
#include <cnip_mdns.h>

//#include <esp_eth.h>
//#include <esp_wifi_internal.h>
#include <esp_private/wifi.h>
#include <string.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi_netif.h>

#if defined(ENABLE_CNIP_TFTP_SERVER)
#include "cnip_tftp.h"
#endif

volatile uint32_t  packet_stopwatch;
//static esp_eth_handle_t s_eth_handle = NULL;

static const char * tag __attribute__((used)) = "tcpip_adapter";
#define NUM_DEVS 3

cnip_hal haldevs[NUM_DEVS]; //index with esp_interface_t


const esp_netif_netstack_config_t *_g_esp_netif_netstack_default_eth      = (void*)&haldevs[2];
const esp_netif_netstack_config_t *_g_esp_netif_netstack_default_wifi_sta = (void*)&haldevs[0];
const esp_netif_netstack_config_t *_g_esp_netif_netstack_default_wifi_ap  = (void*)&haldevs[1];
uint8_t  interface_up[NUM_DEVS];
/*
	typedef enum {
		ESP_IF_WIFI_STA = 0,
		ESP_IF_WIFI_AP,
		ESP_IF_ETH,
		ESP_IF_MAX
	} esp_interface_t;
*/

//Why doesn't weak aliasing seem to work here?
void cnip_user_init( cnip_hal * hal );
void cnip_user_timed_tick( cnip_hal * hal, int is_slow );
void cnip_handle_udp( cnip_ctx * ctx, uint16_t len );
uint8_t cnip_tcp_recv_syn_cb( struct cnip_ctx_t * ctx, uint16_t portno );
uint8_t cnip_tcp_recv_data_cb( cnip_tcp * tcp, int sockno, uint16_t totallen );
void cnip_tcp_connection_closing_cb( cnip_tcp * tcp, int sockno );


void cnip_task( void * pvParameters )
{
	int i;
	TickType_t last_tick = xTaskGetTickCount();


    for( ;; )
    {		
		TickType_t current_tick = xTaskGetTickCount();
		if( current_tick - last_tick > 20 )
		{
			for( i = 0; i < 3; i++ )
			{
				cnip_hal * hal = &haldevs[i];
				if( interface_up[i] )
				{
					//Perform slow ops
					xSemaphoreTake( hal->host_lock, portMAX_DELAY );
					cnip_tick_dhcp( hal->ip_ctx );
					cnip_user_timed_tick( &haldevs[i], 1 );
					cnip_tcp_tick( hal->ip_ctx );


#if defined(ENABLE_CNIP_TFTP_SERVER)
					TickCNIP_TFTP();
#endif

#ifdef CNIP_MDNS
					cnip_mdns_tick( hal->ip_ctx );
#endif

#if 0
					printf( "T:" );
					int j;
					for( j = 0; j < 10; j++ )
						printf( "%d(%d) ", hal->ip_ctx->TCPs[j].state, hal->ip_ctx->TCPs[j].time_since_sent );
					printf( "\n" );
#endif
					xSemaphoreGive( hal->host_lock );
				}
			}
			last_tick = current_tick;
		}

		for( i = 0; i < 3; i++ )
		{
			if( interface_up[i] )
			{
				cnip_hal * hal = &haldevs[i];
				xSemaphoreTake( hal->host_lock, portMAX_DELAY );
				cnip_user_timed_tick( hal, 0 );
				xSemaphoreGive( hal->host_lock );
			}
		}
		//Perform high-frequency ops.
		vTaskDelay(1);
    }
}



void tcpip_adapter_init(void)
{
	xTaskCreate( cnip_task,
         "cnip",
         2048, //Should not need to store an MTU.
         0,
         1,
         0
       );

#if 0
	xTaskCreatePinnedToCore(
                    cnip_task,   /* Function to implement the task */
                    "cnip", /* Name of the task */
                    2048,      /* Stack size in words */
                    NULL,       /* Task input parameter */
                    0,          /* Priority of the task */
                    NULL,       /* Task handle. */
                    0 );  /* Core where the task should run */
#endif

}


//XXX TODO: Handle esp_wifi_internal_set_sta_ip

static esp_err_t CNIP_IRAM send_cnip( cnip_hal * hal, void * buffer, uint16_t len )
{
	xSemaphoreTake( hal->host_lock, portMAX_DELAY );
	hal->incoming_cur = hal->incoming_base = buffer;
	hal->incoming_end = buffer + len;
	packet_stopwatch = GetCycleCount();
	cnip_hal_receivecallback( hal );
	xSemaphoreGive( hal->host_lock );
	return ESP_OK;
}

esp_err_t CNIP_IRAM tcpip_adapter_eth_input(void *buffer, uint16_t len, void *eb)
{
	int ret = 0;
	if( interface_up[ESP_IF_ETH] )
		ret = send_cnip( &haldevs[ESP_IF_ETH], buffer, len );
	esp_wifi_internal_free_rx_buffer(eb);
	return ret;
}

esp_err_t CNIP_IRAM tcpip_adapter_sta_input(void *buffer, uint16_t len, void *eb)
{
	int ret = 0;

	if( interface_up[ESP_IF_WIFI_STA] )
		ret = send_cnip( &haldevs[ESP_IF_WIFI_STA], buffer, len );
	esp_wifi_internal_free_rx_buffer(eb);
	return ret;
}

esp_err_t CNIP_IRAM tcpip_adapter_ap_input(void *buffer, uint16_t len, void *eb)
{
	int ret = 0;

	if( interface_up[ESP_IF_WIFI_AP] )
		ret = send_cnip( &haldevs[ESP_IF_WIFI_AP], buffer, len );
	esp_wifi_internal_free_rx_buffer(eb);
	return ret;
}


int esp_eth_tx( cnip_mem_address start, uint16_t len );

/* For sending things the other way... */
int8_t CNIP_IRAM cnip_hal_xmitpacket( cnip_hal * hal, cnip_mem_address start, uint16_t len )
{
	int8_t ret;

	//This is where we pass our packet back to wifi.

	if( hal->host == (void*)ESP_IF_ETH )
        ret = -1;//return esp_eth_transmit(s_eth_handle, start, len);
	else
	{
		ret = esp_wifi_internal_tx( (wifi_interface_t)hal->host, start, len );
	}
	return ret;
}


/* Calls from IP stack */

void cnip_got_dhcp_lease_cb( cnip_ctx * ctx )
{
	//Got IP!
	uint8_t * ip = (uint8_t*)&ctx->ip_addr;
	printf( "IP: %d.%d.%d.%d  %08x\n", ip[0], ip[1], ip[2], ip[3], ctx->ip_addr );

	//Code elsewhere relies on the ORDERING of these three!
	system_event_t event = { 0 };
	event.event_id = SYSTEM_EVENT_STA_GOT_IP;
	event.event_info.got_ip.ip_info.ip.addr      = ctx->ip_addr;
	event.event_info.got_ip.ip_info.netmask.addr = ctx->ip_mask;
	event.event_info.got_ip.ip_info.gw.addr      = ctx->ip_gateway;
	esp_event_send(&event);
}

static void tcpip_adapter_action_sta_connected(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    uint8_t mac[6];
	tcpip_adapter_ip_info_t ipinfo;
//    wifi_netif_driver_t driver = esp_netif_get_io_driver(esp_netif);
//	esp_wifi_get_if_mac(driver, mac );
	esp_wifi_get_mac( WIFI_IF_STA, mac );
	tcpip_adapter_sta_start(mac, &ipinfo);

	interface_up[ESP_IF_WIFI_STA] = 1;
}

static void tcpip_adapteraction_sta_disconnected(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
	//Tell it to reconnect? Is this needed?
    esp_wifi_connect();
	interface_up[ESP_IF_WIFI_STA] = 0;
}

static void wifi_create_and_start_sta(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data)
{
	esp_wifi_internal_reg_rxcb( WIFI_IF_STA, tcpip_adapter_sta_input );
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, tcpip_adapter_action_sta_connected, NULL);
	esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, tcpip_adapteraction_sta_disconnected, NULL);
	esp_wifi_connect();

	//XXX This is a half-hearted attempt overall, we actually need to register more events and handle their failure.
}

static void wifi_create_and_start_ap(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data)
{
    uint8_t mac[6];
	tcpip_adapter_ip_info_t ipinfo;
 // wifi_netif_driver_t driver = esp_netif_get_io_driver(esp_netif);
 //	esp_wifi_get_if_mac(driver, mac );
	esp_wifi_get_mac( WIFI_IF_AP, mac );
	tcpip_adapter_ap_start(mac, &ipinfo);
	esp_wifi_internal_reg_rxcb( WIFI_IF_AP, tcpip_adapter_ap_input );
}


esp_err_t tcpip_adapter_compat_start_eth(void* eth_driver)
{
	printf( "tcpip_adapter_compat_start_eth!!\n" );
    return ESP_OK;
}


esp_err_t tcpip_adapter_set_default_wifi_handlers(void)
{
    // create instances and register default handlers only on start event
    esp_err_t err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, wifi_create_and_start_sta, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, wifi_create_and_start_ap, NULL);
    if (err != ESP_OK) {
        return err;
    }

	/*
		TODO: Ethernet?
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL));
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
#if CONFIG_EXAMPLE_USE_INTERNAL_ETHERNET
    mac_config.smi_mdc_gpio_num = CONFIG_EXAMPLE_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_EXAMPLE_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
#if CONFIG_EXAMPLE_ETH_PHY_IP101
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_RTL8201
    esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_LAN8720
    esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
#elif CONFIG_EXAMPLE_ETH_PHY_DP83848
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#endif
#elif CONFIG_EXAMPLE_USE_DM9051
    gpio_install_isr_service(0);
    spi_device_handle_t spi_handle = NULL;
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_EXAMPLE_DM9051_MISO_GPIO,
        .mosi_io_num = CONFIG_EXAMPLE_DM9051_MOSI_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_DM9051_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_EXAMPLE_DM9051_SPI_HOST, &buscfg, 1));
    spi_device_interface_config_t devcfg = {
        .command_bits = 1,
        .address_bits = 7,
        .mode = 0,
        .clock_speed_hz = CONFIG_EXAMPLE_DM9051_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_EXAMPLE_DM9051_CS_GPIO,
        .queue_size = 20
    };
    ESP_ERROR_CHECK(spi_bus_add_device(CONFIG_EXAMPLE_DM9051_SPI_HOST, &devcfg, &spi_handle));
    //dm9051 ethernet driver is based on spi driver
    eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(spi_handle);
    dm9051_config.int_gpio_num = CONFIG_EXAMPLE_DM9051_INT_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_dm9051(&phy_config);
#endif
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    config.stack_input = pkt_eth2wifi;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));
    esp_eth_ioctl(s_eth_handle, ETH_CMD_S_PROMISCUOUS, (void *)true);
    esp_eth_start(s_eth_handle);
*/
    return ESP_OK;
}

esp_err_t tcpip_adapter_clear_default_wifi_handlers(void)
{
    // No action if tcpip-adapter compatibility enabled, but interfaces created/configured with esp-netif
    return ESP_OK;
}



//We need to do some device resetting here.
esp_err_t tcpip_adapter_eth_start(uint8_t *mac, tcpip_adapter_ip_info_t *ip_info)
{
	printf( "Starting ETH adapter.\n" );
	cnip_hal * hal = &haldevs[ESP_IF_ETH];
	cnip_hal_init( hal, mac );
	hal->host = (void*)ESP_IF_ETH;
	hal->host_lock = xSemaphoreCreateBinary( );
	xSemaphoreGive( hal->host_lock );
	memset( ip_info, 0, sizeof( *ip_info) );
	tcpip_adapter_set_ip_info( ESP_IF_ETH, ip_info );
	cnip_init_ip( hal->ip_ctx );
	cnip_user_init( hal );
	return ESP_OK;
}

esp_err_t tcpip_adapter_sta_start(uint8_t *mac, tcpip_adapter_ip_info_t *ip_info)
{
	printf( "Starting STA adapter.\n" );
	cnip_hal * hal = &haldevs[ESP_IF_WIFI_STA];
	cnip_hal_init( hal, mac );
	hal->host = (void*)ESP_IF_WIFI_STA;
	hal->host_lock = xSemaphoreCreateBinary( );
	xSemaphoreGive( hal->host_lock );
	memset( ip_info, 0xff, sizeof( *ip_info) );
	tcpip_adapter_set_ip_info( ESP_IF_WIFI_STA, ip_info );
	cnip_init_ip( hal->ip_ctx );
	cnip_dhcpc_create( hal->ip_ctx );
	cnip_user_init( hal );
	return ESP_OK;
}

esp_err_t tcpip_adapter_ap_start(uint8_t *mac, tcpip_adapter_ip_info_t *ip_info)
{
	printf( "Starting AP adapter\n" );
	cnip_hal * hal = &haldevs[ESP_IF_WIFI_AP];

	cnip_hal_init( hal, mac );
	hal->host = (void*)ESP_IF_WIFI_AP;
	hal->host_lock = xSemaphoreCreateBinary( );
	xSemaphoreGive( hal->host_lock );
	memset( ip_info, 0, sizeof( *ip_info) );
	tcpip_adapter_set_ip_info( ESP_IF_WIFI_AP, ip_info );
	cnip_ctx * ctx = hal->ip_ctx;
	cnip_init_ip( ctx );
	ctx->ip_addr = CNIPIP( 192, 168, 4, 1 );
	ctx->ip_mask = CNIPIP( 255, 255, 255, 0 );
	ctx->ip_gateway = CNIPIP( 192, 168, 4, 1 );
	cnip_dhcps_create( ctx );

	//This interface doesn't get the adapter_up flag.
	interface_up[ESP_IF_WIFI_AP] = 1; 

	cnip_user_init( hal );
	return ESP_OK;
}


esp_err_t tcpip_adapter_stop(tcpip_adapter_if_t tcpip_if)
{
	//TODO: add a teardown.
	return ESP_OK;
}

esp_err_t tcpip_adapter_up(tcpip_adapter_if_t tcpip_if)   { printf( "IF %d UP\n", tcpip_if ); interface_up[tcpip_if] = 1; return ESP_OK; }
esp_err_t tcpip_adapter_down(tcpip_adapter_if_t tcpip_if) { printf( "IF %d DOWN\n", tcpip_if ); interface_up[tcpip_if] = 0; return ESP_OK; }

esp_err_t tcpip_adapter_get_ip_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_ip_info_t *ip_info)
{
	if( haldevs[tcpip_if].ip_ctx )
	{
		memcpy( ip_info, &haldevs[tcpip_if].ip_ctx->ip_addr, sizeof( *ip_info ) ); 
		return ESP_OK;
	}
	else
	{
		//For some reason this is normally called before initialization, so we have to fail out of here.
		return ESP_ERR_NOT_FOUND;
	}
}

esp_err_t tcpip_adapter_set_ip_info(tcpip_adapter_if_t tcpip_if, const tcpip_adapter_ip_info_t *ip_info)
{
	memcpy( &haldevs[tcpip_if].ip_ctx->ip_addr, ip_info, sizeof( *ip_info ) ); 
	return ESP_OK;
}

esp_err_t tcpip_adapter_set_dns_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_dns_type_t type, tcpip_adapter_dns_info_t *dns) { printf( "DNS NNS\n" ); return ESP_ERR_NOT_SUPPORTED; }
esp_err_t tcpip_adapter_get_dns_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_dns_type_t type, tcpip_adapter_dns_info_t *dns) { printf( "DNS NNS\n" ); return ESP_ERR_NOT_SUPPORTED; }

esp_err_t tcpip_adapter_get_old_ip_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_ip_info_t *ip_info)
{
	printf( "tcpip_adapter_get_old_ip_info STUB\n" );
	//XXX TODO
	return ESP_OK;
}

esp_err_t tcpip_adapter_set_old_ip_info(tcpip_adapter_if_t tcpip_if, const tcpip_adapter_ip_info_t *ip_info)
{
	printf( "tcpip_adapter_set_old_ip_info STUB\n" );
	//XXX TODO
	return ESP_OK;
}

esp_err_t tcpip_adapter_create_ip6_linklocal(tcpip_adapter_if_t tcpip_if)
{
	printf( "IPv6 Not supported.\n" );
	return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t tcpip_adapter_get_ip6_linklocal(tcpip_adapter_if_t tcpip_if, ip6_addr_t *if_ip6)
{
	printf( "IPv6 Not supported.\n" );
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t tcpip_adapter_dhcps_get_status(tcpip_adapter_if_t tcpip_if, tcpip_adapter_dhcp_status_t *status)
{
	printf( "tcpip_adapter_dhcps_get_status STUB\n" );
	//XXX TODO
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t tcpip_adapter_dhcps_option(tcpip_adapter_dhcp_option_mode_t opt_op, tcpip_adapter_dhcp_option_id_t opt_id, void *opt_val, uint32_t opt_len)
{
	printf( "tcpip_adapter_dhcps_option STUB\n" );
	//XXX TODO
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t tcpip_adapter_dhcps_start(tcpip_adapter_if_t tcpip_if)
{
	printf( "tcpip_adapter_dhcps_start STUB\n" );
	//XXX TODO
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t tcpip_adapter_dhcps_stop(tcpip_adapter_if_t tcpip_if)
{
	printf( "tcpip_adapter_dhcps_stop STUB\n" );
	//XXX TODO
	return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t tcpip_adapter_dhcpc_get_status(tcpip_adapter_if_t tcpip_if, tcpip_adapter_dhcp_status_t *status)
{
	printf( "tcpip_adapter_dhcpc_get_status STUB\n" );
	//XXX TODO
	return ESP_OK;
}

esp_err_t tcpip_adapter_dhcpc_option(tcpip_adapter_dhcp_option_mode_t opt_op, tcpip_adapter_dhcp_option_id_t opt_id, void *opt_val, uint32_t opt_len)
{
	printf( "tcpip_adapter_dhcpc_option STUB\n" );
	//XXX TODO
	return ESP_OK;
}

esp_err_t tcpip_adapter_dhcpc_start(tcpip_adapter_if_t tcpip_if)
{
	printf( "tcpip_adapter_dhcpc_start STUB\n" );
	//XXX TODO
	return ESP_OK;
}

esp_err_t tcpip_adapter_dhcpc_stop(tcpip_adapter_if_t tcpip_if)
{
	printf( "tcpip_adapter_dhcpc_stop STUB\n" );
	//XXX TODO
	return ESP_OK;
}

esp_interface_t tcpip_adapter_get_esp_if(void *dev)
{
	printf( "tcpip_adapter_get_esp_if STUB\n" );

	//XXX TODO
	//XXX TODO
	return 0;
}

esp_err_t tcpip_adapter_get_sta_list(const wifi_sta_list_t *wifi_sta_list, tcpip_adapter_sta_list_t *tcpip_sta_list)
{
	printf( "tcpip_adapter_get_sta_list STUB\n" );

	return ESP_ERR_NOT_SUPPORTED;
}


