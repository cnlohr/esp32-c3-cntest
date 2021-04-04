#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_log.h>
#include "tcpip_adapter.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include <cnip_http.h>
#include <cnip_mdns.h>
#include <driver/rmt.h>
#include <driver/adc_common.h>
#include <driver/gpio.h>


static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

static const char *TAG = "esp82xx_test";


//I don't think this is used anymore.
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
		ESP_LOGI( TAG, "SYSTEM_EVENT_STA_START event\n" );
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        ESP_LOGI( TAG, "ip: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
        ESP_LOGI( TAG, "netmask: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.netmask));
        ESP_LOGI( TAG, "gw: " IPSTR, IP2STR(&event->event_info.got_ip.ip_info.gw));
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
		ESP_LOGI( TAG, "SYSTEM_EVENT_STA_DISCONNECTED event\n" );
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

#if 1
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config = {
        .sta = {
            .ssid = "Cabin2.4",
			.password = "waterfalls9",
            .bssid_set = false
        }
    };
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
#else
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	wifi_config_t wifi_config = {
		.ap = {
			.ssid = "esp32",
			.password = "password",
			.channel = 0,
			.authmode = WIFI_AUTH_WPA2_PSK,
			.ssid_hidden = 0,
			.max_connection = 4,
			.beacon_interval = 100
		}
	};
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
#endif

    ESP_ERROR_CHECK( esp_wifi_start() );
}






CNIP_IRAM int HandleCommand( uint8_t * data, int len, uint8_t * ds )
{
	uint8_t * dsstart = ds;
	data[len] = 0;

	switch (data[0] )
	{
	case 'w':
	{
		ds += sprintf( (char*)ds, "wx012345" );
		break;
	}
	case 'B':
	{
		ds += sprintf( (char*)ds, "BL012345" );
		break;
	}
	case 'I':
	{
		//int r1 = 0, g1 = 0, b1 = 0, r2 = 0, g2 = 0, b2 = 0;
		//int il = sscanf( (char*)data + 1, "%d %d %d %d %d %d", &r1, &g1, &b1, &r2, &g2, &b2 );
		//printf( "DD: %s (%d %d %d %d %d %d ) = %d\n", (char*)data, r1, g1, b1, r2, g2,b2, il );
		ds += sprintf( (char*)ds, "%d %d %d %d %d %d", -1, -1, -1, -1, -1, -1  );
		break;
	}
	default:
		printf( "COMMAND: %s\n", data );
		break;
	}

	return ds - dsstart;	 
}


void app_main()
{
    nvs_flash_init();
	initialise_wifi();

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI( TAG, "This is %s chip with %d CPU cores, WiFi%s%s, ",
			CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGI( TAG, "silicon revision %d, ", chip_info.revision);

    ESP_LOGI( TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	AddMDNSName(    "esp82xx" );
	AddMDNSName(    CONFIG_IDF_TARGET );
	AddMDNSService( "_http._tcp",    "esp82xx webserver", 80 );

	while(1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
