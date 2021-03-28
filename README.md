# ESP32C3 Various tests



## GPIO General observations:
	* Writing out to a port is a quantized operation.
	* CPU is at 160? MHz.
	* AHB quantized at 80 MHz.
	* Nonsequentail Sequential writes take 50ns (4x 80 MHz cycles).
	* Sequential writes are exactly the same (write operation appears to be completely blocking)
	* Sequential reads take 38ns (3x 80MHz cycles)
	* Write and read (But not use data from read) takes 7x 80 MHz cycles.
	* Write, read, write (using data immediately) takes 12 cycles.
	* It appears using data immediately after read incurs a one 80 MHz cycle penalty.
	* W1TS and W1TC are the same as OUT.
	* See gpio_struct.h for the actual structure.

## Get an assembly listing of your program.

Ideally you write some stub function to do what you want to do roughly, then run this, then write your own function to replace it in assembly.

```
riscv32-esp-elf-objdump -S build/hello-world.elf  > list.txt
```


## Rom dumping

```
esptool.py --baud 1500000 -p /dev/ttyUSB0 dump_mem 0x40000000 0x40000 esp32_c3_romdump.bin
riscv32-esp-elf-objdump -b binary --adjust-vma=0x40000000  esp32_c3_romdump.bin -m riscv:rv32 -D > esp32_c3_romdump.txt
```

Find your function in esp32c3.rom.ld and follow it around.


## TODO
 * Play with BBPLL to see if we can up the main clock frequency.  Look up `rtc_clk_cpu_freq_mhz_to_config`


## Process for figuring out I2C and APLL

This describes a process which took place sfrom Sat Mar 27, 2021 at ~4:00 PM to Sunday at ~6:00 AM ET.

 * Look at https://github.com/cnlohr/esp32-cnlohr-demo/blob/master/main/i2s_stream_fast.c and make a system.
 * Ok, outputting MCLK.  Nothing else.  But Now, I'm curious about APLL.

 * Look around for APLL references.
 * Look at other devices for their APLL config code.
 * Find out I need to do something like this, but it's broken and causes ESP to reboot.
```
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM2, sdm2);
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM0, sdm0);
        REGI2C_WRITE_MASK(I2C_APLL, I2C_APLL_DSDM1, sdm1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_1);
        REGI2C_WRITE(I2C_APLL, I2C_APLL_SDM_STOP, APLL_SDM_STOP_VAL_2_REV1);
```
 * Other addresses don't reboot.
 * Let's explore the internal I2C.
 * `rom_i2c_readReg` crashes when trying to access what is in the headers as the APLL address.
 * Dump rom, let's figure out how it works.
 * Copy pertinent code into a new assembly function, just avoiding the crashy bits.
 * OK THAT WAS TOO COMPLICATED.
 * Try again
 * It doesn't crash this time for some reason.
 * Dump list of connected I2C devices.
```
//61 0000001f 0000001f 0000001f Brown Out Detector
//62 00000000 00000000 00000000 ?
//63 00000000 00000000 00000000 ?
//64 00000000 00000000 00000000 ?
//65 00000000 00000000 00000000 ?
//66 00000018 00000018 00000018 BB PLL
//67 00000000 00000000 00000000 ?
//68 000000ff 000000ff 000000ff
//69 00000000 00000000 00000000 SAR ADC
//6a 00000077 00000077 00000077 BIAS
//6b 00000000 00000000 00000000  ?
//6c 000000ff 000000ff 000000ff 
//6d 000000f0 000000f0 000000f0 Possibly APLL
```
 * Looks like it might be the APLL
 * Dump registers on APLL.  Note comments are from ESP32, these may be all bogus.
```
	00: 000000f0 //IR CAL RST, IR CAL START, IR CAL UNSTOP OC_ENB_FCAL
	01: 00000004 //I2C_APLL_IR_CAL_EXT_CAP = 4; I2C_APLL_IR_CAL_ENX_CAP = 0;  I2C_APLL_OC_LBW = 0
	02: 00000080 //I2C_APLL_OC_ENB_VCON = 1
	03: 00000004 //I2C_APLL_OR_CAL_CAP = 4; I2C_APLL_OR_CAL_UDF = 0; I2C_APLL_OR_CAL_OVF = 0; I2C_APLL_OR_CAL_END = 0
	04: 00000097 //I2C_APLL_OR_OUTPUT_DIV = 7;  I2C_APLL_OC_TSCHGP = 0; I2C_APLL_EN_FAST_CAL = 1
	05: 00000017 //I2C_APLL_OC_DHREF_SEL = 3; I2C_APLL_OC_DLREF_SEL = 1; I2C_APLL_SDM_DITHER = 1; I2C_APLL_SDM_STOP = 0; I2C_APLL_SDM_RSTB = 0
	06: 0000009c //I2C_APLL_OC_DVDD = 156
	07: 00000018 //I2C_APLL_DSDM2 = 24
	08: 0000001f //I2C_APLL_DSDM1 = 31
	09: 0000000f //I2C_APLL_DSDM0 = 15
	0a: 00000000
	0b: 00000004
	0c: 00000016
	0d: 00000042
	0e: 00000080
	0f: 00000000
```
 *  = fXtal * (24 + 31/256 + 15/65536 + 4 ) / 18 => fXTal = 153.62MHz? --- This APLL is different :(
 * Ok, looks like the APLL is in use.  Don't mess with it.
 * Try configuring MCLK on I2S to APLL
 * Find out it's APLL/2; currently outputting at 120 MHz.
 * Yes, it looks like the APLL is currently in use for something.  Maybe it can be tweaked?
 * Tweak SDM0 -- NADA! Nothing seems to get it to go.  Ok.  Give up on that for now.

OK GIVE UP ON APLL.

 * Just chug away at using the I2S engine.  Hopefully we can get the bitrate up.
 * HURRRRRRR It looks like some pretty major changes in the way the data is handled.  Need to do gdma.
 * Ok, dig around, a lot.  Find the SPI stuff with GDMA and try to piece it together.
 * Spend 5 hours poking at registers.
 * Get BCLK up.
 * It's really useful to force the clocks on to experiment.  Also TDM mode seems to make clock more when things are janky.
 * Rifle around til 6:06 AM.
 * Get it working at 120 Mbauds.


