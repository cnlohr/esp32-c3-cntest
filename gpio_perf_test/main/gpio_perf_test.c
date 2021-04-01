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

void IRAM_ATTR gpio_test_interrupt( void * attr )
{
	GPIO.out.val = 1<<3;
	GPIO.status_w1tc.val = 0xffffffff;
}

void IRAM_ATTR asm_isr_handler( void * attr );

void app_main(void)
{
    printf("Hello world!\n");


	extern int _custom_vector_table;
	cpu_hal_set_vecbase(&_custom_vector_table);


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
    io_conf.pin_bit_mask = ( 1<<0 ) | ( 1<<1 ) | ( 1<<2 ) | ( 1<<3 ) | ( 1<<4 ) | (1<<5) | ( 1<<6 );
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);


	//Interrupt test assumes GPIO6 and 7 are tied together.
	//Alternative way to configure GPIOs

    gpio_pad_select_gpio(GPIO_NUM_7);
    gpio_set_direction(GPIO_NUM_7, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_7, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(GPIO_NUM_7, GPIO_INTR_POSEDGE);
    gpio_intr_enable(GPIO_NUM_7);

// Fast path (920ns)
    gpio_isr_register( asm_isr_handler, 0, ESP_INTR_FLAG_FIXED | (31<<ESP_INTR_FLAG_FIXED_OFF_S), 0 );
// Slow path (1350ns)
//    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
//    gpio_isr_handler_add(GPIO_NUM_7, gpio_test_interrupt, 0);

//Not sure where the NMI is connected.
//	GPIO.pcpu_nmi_int.val = (1<<7);
//	GPIO.pcpu_int.val = (1<<7);



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

	//Technique: Use a logic analyzer or oscilloscope on GPIO0 and count cycles.

	//Just FYI USB is 13.33x 160MHz Clock Ticks.
	//GPIO Input takes 6 clock cycles.
	//That means we have 7.3333 clock cycles to figure out what to do.

	printf( "Entering main loop\n" );
	while(1)
	{
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		*odr = 1;
		*odr = 0;
		*odr = 1;
		//This section takes 3.76us or 376ns per hit.  That's 6x 160 MHz clock ticks.
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
		*odr = 0;
		*odr = 1;
		//This section takes 5.052us or 505.2ns per hit.  That's 8x 160 MHz clock ticks.
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;asm volatile( "nop\n" ); temp = *readval;
		*odr = 0;

		*odr = 1;
		//This section ALSO takes 5.052us or 505.2ns per hit.  That's 8x 160 MHz clock ticks.
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\n" ); temp = *readval;
		*odr = 0;

		*odr = 1;
		//This section ALSO takes 6.3us or 630ns per hit.  That's 10x 160 MHz clock ticks.
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;asm volatile( "nop\nnop\nnop\n" ); temp = *readval;
		*odr = 0;


		asm volatile( "nop\nnop" );
		*odr = *readval;
		asm volatile( "nop\nnop" );
		*odr = 0;
		asm volatile( "nop\nnop" );
		*odr = 1;
		asm volatile( "nop\nnop" );
		*odr = 0;
		asm volatile( "nop\nnop\nnop" );
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

		GPIO.out.val = 1<<1;


		//Triggers GPIO interrupt on GPIO7 if you solder GPIO6 and GPIO7 together.
		*odr = (1<<6);

		//Tested:  It takes this many nops sothat 
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );

		//Interrupt is triggered here.
		*odr = 1;
		*odr = 1;
		*odr = 1;
		*odr = 1;
		*odr = 1;
		*odr = 1;
		*odr = 1;
		*odr = 1;
		*odr = 1;
		*odr = 1;
		*odr = 1;

		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );
		asm volatile( "nop\nnop\nnop\nnop" );

	}

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
