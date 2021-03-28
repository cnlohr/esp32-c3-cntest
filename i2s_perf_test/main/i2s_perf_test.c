// Uses the I2S engine on the ESP32-C3 to output at 120 Mbaud.

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "freertos/queue.h"
#include "esp_private/gdma.h"
#include "esp32c3/rom/lldesc.h"
#include "soc/gpio_struct.h"
#include "hal/gdma_ll.h"
#include "soc/rtc.h" //for APLL
#include "regi2c_ctrl.h"
#include "i2c_rtc_clk.h"

#define I2C_APLL_IR_CAL_DELAY        0
#define I2C_APLL_IR_CAL_DELAY_MSB    3
#define I2C_APLL_IR_CAL_DELAY_LSB    0

#define I2C_APLL_IR_CAL_RSTB        0
#define I2C_APLL_IR_CAL_RSTB_MSB    4
#define I2C_APLL_IR_CAL_RSTB_LSB    4

#define I2C_APLL_IR_CAL_START        0
#define I2C_APLL_IR_CAL_START_MSB    5
#define I2C_APLL_IR_CAL_START_LSB    5

#define I2C_APLL_IR_CAL_UNSTOP        0
#define I2C_APLL_IR_CAL_UNSTOP_MSB    6
#define I2C_APLL_IR_CAL_UNSTOP_LSB    6

#define I2C_APLL_OC_ENB_FCAL        0
#define I2C_APLL_OC_ENB_FCAL_MSB    7
#define I2C_APLL_OC_ENB_FCAL_LSB    7

#define I2C_APLL_IR_CAL_EXT_CAP        1
#define I2C_APLL_IR_CAL_EXT_CAP_MSB    4
#define I2C_APLL_IR_CAL_EXT_CAP_LSB    0

#define I2C_APLL_IR_CAL_ENX_CAP        1
#define I2C_APLL_IR_CAL_ENX_CAP_MSB    5
#define I2C_APLL_IR_CAL_ENX_CAP_LSB    5

#define I2C_APLL_OC_LBW        1
#define I2C_APLL_OC_LBW_MSB    6
#define I2C_APLL_OC_LBW_LSB    6

#define I2C_APLL_IR_CAL_CK_DIV        2
#define I2C_APLL_IR_CAL_CK_DIV_MSB    3
#define I2C_APLL_IR_CAL_CK_DIV_LSB    0

#define I2C_APLL_OC_DCHGP        2
#define I2C_APLL_OC_DCHGP_MSB    6
#define I2C_APLL_OC_DCHGP_LSB    4

#define I2C_APLL_OC_ENB_VCON        2
#define I2C_APLL_OC_ENB_VCON_MSB    7
#define I2C_APLL_OC_ENB_VCON_LSB    7

#define I2C_APLL_OR_CAL_CAP        3
#define I2C_APLL_OR_CAL_CAP_MSB    4
#define I2C_APLL_OR_CAL_CAP_LSB    0

#define I2C_APLL_OR_CAL_UDF        3
#define I2C_APLL_OR_CAL_UDF_MSB    5
#define I2C_APLL_OR_CAL_UDF_LSB    5

#define I2C_APLL_OR_CAL_OVF        3
#define I2C_APLL_OR_CAL_OVF_MSB    6
#define I2C_APLL_OR_CAL_OVF_LSB    6

#define I2C_APLL_OR_CAL_END        3
#define I2C_APLL_OR_CAL_END_MSB    7
#define I2C_APLL_OR_CAL_END_LSB    7

#define I2C_APLL_OR_OUTPUT_DIV        4
#define I2C_APLL_OR_OUTPUT_DIV_MSB    4
#define I2C_APLL_OR_OUTPUT_DIV_LSB    0

#define I2C_APLL_OC_TSCHGP        4
#define I2C_APLL_OC_TSCHGP_MSB    6
#define I2C_APLL_OC_TSCHGP_LSB    6

#define I2C_APLL_EN_FAST_CAL        4
#define I2C_APLL_EN_FAST_CAL_MSB    7
#define I2C_APLL_EN_FAST_CAL_LSB    7

#define I2C_APLL_OC_DHREF_SEL        5
#define I2C_APLL_OC_DHREF_SEL_MSB    1
#define I2C_APLL_OC_DHREF_SEL_LSB    0

#define I2C_APLL_OC_DLREF_SEL        5
#define I2C_APLL_OC_DLREF_SEL_MSB    3
#define I2C_APLL_OC_DLREF_SEL_LSB    2

#define I2C_APLL_SDM_DITHER        5
#define I2C_APLL_SDM_DITHER_MSB    4
#define I2C_APLL_SDM_DITHER_LSB    4

#define I2C_APLL_SDM_STOP        5
#define I2C_APLL_SDM_STOP_MSB    5
#define I2C_APLL_SDM_STOP_LSB    5

#define I2C_APLL_SDM_RSTB        5
#define I2C_APLL_SDM_RSTB_MSB    6
#define I2C_APLL_SDM_RSTB_LSB    6

#define I2C_APLL_OC_DVDD        6
#define I2C_APLL_OC_DVDD_MSB    4
#define I2C_APLL_OC_DVDD_LSB    0

#define I2C_APLL_DSDM2        7
#define I2C_APLL_DSDM2_MSB    5
#define I2C_APLL_DSDM2_LSB    0

#define I2C_APLL_DSDM1        8
#define I2C_APLL_DSDM1_MSB    7
#define I2C_APLL_DSDM1_LSB    0

#define I2C_APLL_DSDM0        9
#define I2C_APLL_DSDM0_MSB    7
#define I2C_APLL_DSDM0_LSB    0


#define APLL_SDM_STOP_VAL_1         0x09
#define APLL_SDM_STOP_VAL_2_REV0    0x69
#define APLL_SDM_STOP_VAL_2_REV1    0x49
#define APLL_CAL_DELAY_1            0x0f
#define APLL_CAL_DELAY_2            0x3f
#define APLL_CAL_DELAY_3            0x1f

//Specifically from C3, based on headers
//#define I2C_BIAS            0X6A
//#define I2C_BIAS_HOSTID     0
//#define I2C_BOD            0x61
//#define I2C_BOD_HOSTID     0
//#define I2C_BBPLL           0x66
//#define I2C_BBPLL_HOSTID    0
//#define I2C_DIG_REG 0x6D
//#define I2C_DIG_REG_HOSTID 0
//#define I2C_SAR_ADC            0X69
//#define I2C_SAR_ADC_HOSTID     0

//From S3
//#define I2C_APLL            0X6D
//#define I2C_APLL_HOSTID     1

//I don't think this is the APLL.
#define I2C_APLL            0x6D
#define I2C_APLL_HOSTID     0

void rtc_clk_apll_enable(bool enable, uint32_t sdm0, uint32_t sdm1, uint32_t sdm2, uint32_t o_div)
{
	//XXX DOES NOT WORK!

	int i; i = 0;
//    REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PD, enable ? 0 : 1);
//    REG_SET_FIELD(RTC_CNTL_ANA_CONF_REG, RTC_CNTL_PLLA_FORCE_PU, enable ? 1 : 0);

	//Checked address:
	//0x00..0x5F = Empty see readme for others.
#if 0
	for( i = 0x60; i < 0x7f; i++ )
	{
		uint32_t reg0 = rom_i2c_readReg(i, 0, 0 );
		uint32_t reg1 = rom_i2c_readReg(i, 1, 0 );
		uint32_t reg2 = rom_i2c_readReg(i, 2, 0 );
		printf( "%02x %08x %08x %08x\n",i, reg0, reg1,reg2 );
	}
#endif

        REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_2_REV1);

        REGI2C_WRITE(I2C_APLL, I2C_APLL_IR_CAL_DELAY, APLL_CAL_DELAY_1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_IR_CAL_DELAY, APLL_CAL_DELAY_2);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_IR_CAL_DELAY, APLL_CAL_DELAY_3);

#if 1
	for( i = 0; i < 32; i++ )
	{
		printf( "%02x: %02x\n", i,rom_i2c_readReg( I2C_APLL, I2C_APLL_HOSTID, i ) ); 
	}
#endif
#if 0
    if (enable) {
printf( "MARKERD\n" );
		//This seems to crash the chip
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM2, sdm2);
printf( "MARKERD.5\n" );
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM0, sdm0);
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM1, sdm1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_2_REV1);
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_OR_OUTPUT_DIV, o_div);
printf( "MARKERE\n" );

        /* calibration */
        REGI2C_WRITE(I2C_APLL, I2C_APLL_IR_CAL_DELAY, APLL_CAL_DELAY_1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_IR_CAL_DELAY, APLL_CAL_DELAY_2);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_IR_CAL_DELAY, APLL_CAL_DELAY_3);
printf( "MARKERF\n" );

        /* wait for calibration end */
//        while (!(REGI2C_READ_MASK(I2C_APLL, I2C_APLL_OR_CAL_END))) {
//            /* use esp_rom_delay_us so the RTC bus doesn't get flooded */
//            esp_rom_delay_us(1);
//        }
    }
#endif
}

uint32_t my_asm_func( int num, int numx2, int numx4 );



int callbacksT;
int callbacksR;

lldesc_t * i2s_tx_descriptors;//[3];
lldesc_t * i2s_rx_descriptors;//[3];

gdma_channel_handle_t tx_channel;
gdma_channel_handle_t rx_channel;


//Note maximum LLDESC transfer size is 4096 units? (are units bytes?)

#define I2S_XFER_AMOUNT 1024

volatile uint32_t i2s_tx_data[3][I2S_XFER_AMOUNT];
volatile uint32_t i2s_rx_data[3][I2S_XFER_AMOUNT];


bool my_gdma_event_callback(gdma_channel_handle_t dma_chan, gdma_event_data_t *event_data, void *user_data)
{
	(*((uint32_t*)user_data))++;
	return true;
}

gdma_tx_event_callbacks_t gdma_callbacksTX;
gdma_rx_event_callbacks_t gdma_callbacksRX;



void app_main(void)
{
	int i; i = 0;
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

	printf( "My ASM Test: %d\n", my_asm_func( 1, 2, 3 ) );


    gpio_num_t pins[] = {
		0,
		1,
		2,
		3,
    };

    gpio_config_t conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
    };
    for (int i = 0; i < sizeof(pins)/sizeof(gpio_num_t); ++i) {
        conf.pin_bit_mask = 1LL << pins[i];
        gpio_config(&conf);
    }

    gpio_matrix_out(GPIO_NUM_0, I2S_MCLK_OUT_IDX, 0, 0);
    gpio_matrix_out(GPIO_NUM_1, I2SO_BCK_OUT_IDX, 0, 0);
    gpio_matrix_out(GPIO_NUM_2, I2SO_WS_OUT_IDX, 0, 0);
    gpio_matrix_out(GPIO_NUM_3, I2SO_SD_OUT_IDX, 0, 0);

	periph_module_enable(PERIPH_GDMA_MODULE);
	periph_module_enable(PERIPH_I2S1_MODULE);

	printf( "I2S Engine Version: %08x\n", I2S1.date.val );


	printf( "1======================\n" );
	I2S1.rx_conf.val = 7; //Reset module.
	I2S1.tx_conf.val = 3; //Reset module.

	I2S1.tx_clkm_conf.val = 0;
	I2S1.rx_clkm_conf.val = 0;
	I2S1.rx_conf1.val = 0;
	I2S1.tx_conf1.val = 0;
	I2S1.rx_clkm_div_conf.val = 0;
	I2S1.tx_clkm_div_conf.val = 0;
	I2S1.lc_hung_conf.val = 0;
	I2S1.tx_pcm2pdm_conf.val = 0;
	I2S1.tx_pcm2pdm_conf1.val = 0;
	I2S1.rx_tdm_ctrl.val = 0;
	I2S1.tx_tdm_ctrl.val = 0;

	I2S1.tx_timing.val = 0; //Disable all timing delay
	I2S1.rx_timing.val = 0; //Disable all timing delay


	//GDMA TIME
	{
		i2s_tx_descriptors = heap_caps_malloc(sizeof(lldesc_t) * 3, MALLOC_CAP_DMA);
		i2s_rx_descriptors = heap_caps_malloc(sizeof(lldesc_t) * 3, MALLOC_CAP_DMA);

		for( i = 0; i < 3; i++ )
		{
			int j;
			i2s_tx_descriptors[i].length = I2S_XFER_AMOUNT;
			i2s_tx_descriptors[i].owner = 1;		//LLDESC_HW_OWNED = 1
			i2s_tx_descriptors[i].sosf = 0;
			i2s_tx_descriptors[i].eof = 1; //Maybe should be 1?
			i2s_tx_descriptors[i].offset = 0;
			i2s_tx_descriptors[i].buf = (volatile uint8_t*)&i2s_tx_data[i][0];
			i2s_tx_descriptors[i].qe.stqe_next = &i2s_tx_descriptors[(i+1)%3];
			for( j = 0; j < I2S_XFER_AMOUNT; j++ )
			{
				i2s_tx_data[i][j] = (i==0)?0xffffffff:0xaaaaaaaa;
			}

			i2s_rx_descriptors[i].length = I2S_XFER_AMOUNT;
			i2s_rx_descriptors[i].owner = 1;
			i2s_rx_descriptors[i].sosf = 0;
			i2s_rx_descriptors[i].eof = 1; //Maybe should be 1?
			i2s_rx_descriptors[i].offset = 0;
			i2s_rx_descriptors[i].buf = (volatile uint8_t*)&i2s_rx_data[i][0];
			i2s_rx_descriptors[i].qe.stqe_next = &i2s_rx_descriptors[(i+1)%3];
			for( j = 0; j < I2S_XFER_AMOUNT; j++ )
			{
				i2s_rx_data[i][j] = 0xaaaaaaaa;
			}
		}

		gdma_callbacksTX.on_trans_eof = my_gdma_event_callback;
		gdma_callbacksRX.on_recv_eof = my_gdma_event_callback;
		
		gdma_channel_alloc_config_t tx_alloc_config = {
		    .flags.reserve_sibling = 1,
		    .direction = GDMA_CHANNEL_DIRECTION_TX,
		};
		gdma_channel_alloc_config_t rx_alloc_config = {
		    .direction = GDMA_CHANNEL_DIRECTION_RX,
		    .sibling_chan = tx_channel,
		};

		printf( "gdma_new_channel = %d\n", gdma_new_channel(&tx_alloc_config, &tx_channel) );
		printf( "gdma_new_channel = %d\n", gdma_new_channel(&rx_alloc_config, &rx_channel) );

		printf( "gdma_connect = %d\n", gdma_connect(rx_channel, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_I2S, 0) ));
		printf( "gdma_connect = %d\n", gdma_connect(tx_channel, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_I2S, 0) ));

		printf( "gdma_register_tx_event_callbacks = %d\n", gdma_register_tx_event_callbacks(tx_channel, &gdma_callbacksTX, &callbacksT));
		printf( "gdma_register_tx_event_callbacks = %d\n", gdma_register_rx_event_callbacks(rx_channel, &gdma_callbacksRX, &callbacksR));

		int out_actual_tx_dma_chan, out_actual_rx_dma_chan;
		printf( "gdma_get_channel_id = %d\n", gdma_get_channel_id(tx_channel, (int *)&out_actual_tx_dma_chan) );
		printf( "gdma_get_channel_id = %d\n", gdma_get_channel_id(rx_channel, (int *)&out_actual_rx_dma_chan) );
		printf( "Channel  Pair: %d %d\n", out_actual_tx_dma_chan, out_actual_rx_dma_chan );
	}

	//Does this work? ... NO ... It causes the chip to crash.
	//rtc_clk_apll_enable(1, 0, 0, 8, 0);

	int dev;
	int hid;
	for( hid = 0; hid < 2; hid++ )
	for( dev = 0x60; dev < 0x6f; dev++ )
	{
		printf( "%02x/%d: ", dev, hid );
		for( i = 0; i < 32; i++ )
		{
			printf( "%02x ", rom_i2c_readReg( dev, hid, i )); 
		}
		printf( "\n" );
	}

	//0x66, 0x00, 0x03, 0x09 controls system clock frequency. (upping it a little)
	//            0x04, 0x5c controls frequency (divides by half) 0x8c goes down to 16 MHz.  controls some big divisor. 0xff = 83.24x2?!?!
	//rom_i2c_writeReg( 0x66, 0x00, 0x03, 0x09 );

	//Back to BBPLL
	//66/0: 18 25 50 08 6b 80 93 00 c1 92 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  BBPLL
	//address 0x01 affects apll but only makes it slower (top nibble)
	//address 0x02 affects apll too.  These may also be affecting system frequency. (only lowers)
	//NOTE: When running the chip at 0x03=0x0c, APLL goes to 320 MHz.
	//rom_i2c_writeReg( 0x66, 0x00, 0x03, 0x0a );
//	rtc_clk_apb_freq_update( 30000000 );
//    rtc_clk_fast_freq_set( 26000000 );

	// TODO Look into this
	/*
	rtc_cpu_freq_config_t cfg;
    bool res = rtc_clk_cpu_freq_mhz_to_config(160, &cfg);
    if (!res) {
        SOC_LOGE(TAG, "invalid CPU frequency value");
        abort();
    }
    rtc_clk_cpu_freq_set_config(&cfg);
*/

	printf( "3======================\n" );

	{
		// CONFIRMED : This is master clock.  You can output this on a pin directly.  It can output at 80 MHz when 160 MHz source is selected.
		//Note that it's 160/(tx_clkm_div_num) - but if you select 0, then it is actually 625 kHz (160 MHz / 256)

		// Integral I2S TX clock divider value. f_I2S_CLK = f_I2S_CLK_S/(N+b/a).
		// There will be (a-b) * n-div and b * (n+1)-div.
		// So the average combination will be:  for b <= a/2  z * [x * n-div + (n+1)-div] + y * n-div. For b > a/2  z * [n-div + x * (n+1)-div] + y * (n+1)-div.*/
		I2S1.tx_clkm_conf.tx_clkm_div_num = 1; //This is the main divisor.  Setting to 0 slows it down.

		// Select I2S Tx module source clock. 0: XTAL clock. 1: APLL. 2: CLK160. 3: I2S_MCLK_in.
		I2S1.tx_clkm_conf.tx_clk_sel = 1;

		//tx_clkm_div_num says:
		//f_I2S_CLK = f_I2S_CLK_S/(N+b/a).
		//	There will be (a-b) * n-div and b * (n+1)-div.
		//  So the average combination will be:  
		//		For b <= a/2  z * [x * n-div + (n+1)-div] + y * n-div. 
		//		For  b > a/2  z * [n-div + x * (n+1)-div] + y * (n+1)-div.
		// Where N is tx_clkm_div_num I believe.


		//	tx_clkm_div_z:  -->  For b <= a/2  the value of I2S_TX_CLKM_DIV_Z is b.         For b > a/2  the value of I2S_TX_CLKM_DIV_Z is (a-b).
		//	tx_clkm_div_y:  -->  For b <= a/2  the value of I2S_TX_CLKM_DIV_Y is (a%b) .    For b > a/2  the value of I2S_TX_CLKM_DIV_Y is (a%(a-b)).
		//	tx_clkm_div_x:  -->  For b <= a/2  the value of I2S_TX_CLKM_DIV_X is (a/b) - 1. For b > a/2  the value of I2S_TX_CLKM_DIV_X is (a/(a-b)) - 1.
		//	tx_clkm_div_yn1: --> For b <= a/2  the value of I2S_TX_CLKM_DIV_YN1 is 0 .      For b > a/2  the value of I2S_TX_CLKM_DIV_YN1 is 1.


		//Let's try making the second part 1/2  b = 1; a = 2 -> This works!
		//I2S1.tx_clkm_div_conf.tx_clkm_div_z = 1;
		//I2S1.tx_clkm_div_conf.tx_clkm_div_y = 0;
		//I2S1.tx_clkm_div_conf.tx_clkm_div_x = 1;
		//I2S1.tx_clkm_div_conf.tx_clkm_div_yn1 = 0;

		//Let's try making the second part 0/2  b = 1; a = 2
		I2S1.tx_clkm_div_conf.tx_clkm_div_z = 0;
		I2S1.tx_clkm_div_conf.tx_clkm_div_y = 0;
		I2S1.tx_clkm_div_conf.tx_clkm_div_x = 1;
		I2S1.tx_clkm_div_conf.tx_clkm_div_yn1 = 0;
	}

	I2S1.tx_conf.tx_pcm_bypass = 1; //CRTIICAL - Without doing this, data gets corrupted.

	I2S1.tx_conf.tx_mono = 1;

	I2S1.tx_conf.tx_tdm_en = 1; //XXX INTERESTING -> TDM is easier to make work it seems.
	I2S1.tx_conf1.tx_tdm_ws_width = 1; // XXX Why no work in TDM if nonzero?  It seems that the word length is falsely very short.

	I2S1.tx_conf.tx_chan_mod = 0; //Dual channel mode?  (From ESP32 TRM)  Note: 1 in some places is referenced as faster.

	I2S1.tx_conf1.tx_bits_mod = 31; //should this be 16?
	I2S1.tx_conf1.tx_bck_no_dly = 1;//????
	I2S1.tx_conf1.tx_tdm_chan_bits = 31;

	I2S1.tx_conf1.tx_bck_div_num = 0; //Works, but weird... Uneven duty cycle on even numbers..  THIS WORKS THOUGH SO DON'T MESS WITH IT.
#if 0
	I2S1.rx_conf1.rx_bck_div_num = 2;
	I2S1.rx_conf1.rx_tdm_ws_width= 31; //eh probs unused.
	I2S1.rx_conf1.rx_bits_mod = 8; //should this be 16?
#endif

	I2S1.tx_tdm_ctrl.tx_tdm_chan0_en = 1;
	I2S1.tx_tdm_ctrl.tx_tdm_chan1_en = 1;
	I2S1.tx_tdm_ctrl.tx_tdm_chan2_en = 1;
	I2S1.tx_tdm_ctrl.tx_tdm_chan3_en = 1;
	I2S1.tx_tdm_ctrl.tx_tdm_tot_chan_num = 3;
	I2S1.tx_tdm_ctrl.tx_tdm_skip_msk_en = 0;

	//Verified no changes with reserved values.
	//I2S1.tx_conf.reserved14 = 1;
	//I2S1.tx_conf.reserved4 = 1;
	//I2S1.tx_conf.reserved21 = 0x7;
	//I2S1.tx_conf.reserved28 = 0x0f;
	//I2S1.tx_conf1.reserved31 = 1;
	//I2S1.tx_clkm_conf.reserved8 = 0x3ffff;
	//I2S1.tx_clkm_conf.reserved30 = 3;
	//I2S1.tx_clkm_div_conf.reserved28 = 0xf;
	//I2S1.tx_tdm_ctrl.reserved21 = 0x07ff;
	//I2S1.reserved_0 = 0xffffffff;
	//I2S1.reserved_4 = 0xffffffff;
	//I2S1.reserved_8 = 0xffffffff;
	//I2S1.reserved_1c = 0xffffffff;
	//I2S1.reserved_48 = 0xffffffff;
	//I2S1.reserved_4c = 0xffffffff;
	//I2S1.reserved_70 = 0xffffffff;
	//I2S1.reserved_74 = 0xffffffff;
	//I2S1.reserved_78 = 0xffffffff;
	//I2S1.reserved_7c = 0xffffffff;



	//Other random stuff
	I2S1.rx_eof_num.val = 1024;

	//Actually enable.
	I2S1.tx_clkm_conf.clk_en = 1;
	I2S1.tx_clkm_conf.tx_clk_active = 1;

	gdma_start( tx_channel, (intptr_t) i2s_tx_descriptors );
	gdma_start( rx_channel, (intptr_t) i2s_rx_descriptors );


	I2S1.tx_conf.tx_stop_en = 1;//XXX INTERESTING	//Disable BCK if FIFO empty (if 1) --> BCK still on, I guess that means FIFO has data ... But this is true, even when GDMA hasn't started.
	I2S1.tx_conf.tx_start = 1;

//Not sure what this does.
	I2S1.tx_conf.tx_update = 1;


	printf( "TXCONF: %08x\n", I2S1.tx_conf.val );
	printf( "TXCONF1: %08x\n", I2S1.tx_conf1.val );


	printf( "6======================\n" );


    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds... MODE: %02x (1 = idle) IS: %02x CALLBACKS: %d %d\n", i, I2S1.state.val, I2S1.int_st.val, callbacksT, callbacksR );
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
