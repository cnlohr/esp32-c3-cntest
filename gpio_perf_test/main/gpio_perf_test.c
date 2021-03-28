/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "freertos/queue.h"

//This is the magic file.
#include "soc/gpio_struct.h"

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());


    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1<<0;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);


	//hw->out_w1ts.out_w1ts
	volatile uint32_t * w1ts = &GPIO.out_w1ts.val;
	volatile uint32_t * w1tc = &GPIO.out_w1tc.val;
	volatile uint32_t * odr = &GPIO.out.val;
	volatile uint32_t * readval = &GPIO.in.val;
	uint32_t temp = 0;
	/*
		General observations:
			* Writing out to a port is a quantized operation.
			* CPU is at 160? MHz.
			* AHB quantized at 80 MHz.
			* Nonsequentail Sequential writes take 50ns (4x 80 MHz cycles).
			* Sequential writes are exactly the same (write operation appears to be completely blocking)
			* Sequential reads take 38ns (3x 80MHz cycles)
			* Write and read (But not use data from read) takes 7x 80 MHz cycles.
			* Write, read, write (using data immediately) takes 12 cycles.
			* It appears using data immediately after read incurs a one 80 MHz cycle penalty.
	*/

	while(1)
	{
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		*odr = 1;
		asm volatile( "nop" );
		*odr = 0;
#if 0
		*odr = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
		temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;temp = *readval;
#endif
		asm volatile( "nop\nnop" );
		*odr = 1;
		asm volatile( "nop\nnop\nnop" );
		*odr = 0;
		asm volatile( "nop\nnop\nnop\nnop" );
		*odr = 1;
		*odr = 0;

		*w1ts = 1;
		*w1tc = 1;
		*w1ts = 1;
		*w1tc = 1;
		*w1ts = 1;
		*w1tc = 1;
		*w1ts = 1;
		*w1tc = 1;

		*odr = 1;
		*odr = 0;
		*odr = 1;
		*odr = 0;
		*odr = 1;
		*odr = 0;
		*odr = 1;
		*odr = 0;
       // gpio_set_level(0, 1);
       // gpio_set_level(0, 0);
	}

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
