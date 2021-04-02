set(COMPONENT_SRCS "main.c")
set(COMPONENT_ADD_INCLUDEDIRS "")

set( COMPONENTS, bootloader bt-support driver esp32 esp_adc_cal
	espcoredump esp_event esp_ringbuf esptool_py heap
	idf_test freertos jsmn json log micro-ecc newlib nvs_flash
	pthread soc spiffs spi_flash ulp unity wpa_supplicant
	app_trace xtensa-debug-module ethernet bootloader_support
	vfs app_update mbedtls main partition_table efuse
	bootloader_support esp_common xtensa esp_rom esp_timer
	esp_system cnip cnhttp )

register_component()
