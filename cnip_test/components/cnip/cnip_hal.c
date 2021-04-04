#include <cnip_hal.h>
#include <cnip_core.h>
#include <string.h>
#include <endian.h>

/*
	This file is specific to the ESP32.
*/

#include <esp_err.h>

uint8_t * cnip_packet_base;
uint8_t * cnip_packet_end;




//return 0 if OK, otherwise nonzero.
int8_t CNIP_IRAM cnip_hal_init( cnip_hal * hal, const unsigned char * macaddy )
{
	printf( "cnip_hal_init( %02x:%02x:%02x:%02x:%02x:%02x )\n", macaddy[0], macaddy[1], macaddy[2], macaddy[3], macaddy[4], macaddy[5] );
	memset( hal, 0, sizeof( *hal ) );
	memcpy( hal->mac, macaddy, 6 );
	hal->ip_ctx = malloc( sizeof( cnip_ctx ) );
	memset( hal->ip_ctx, 0, sizeof( cnip_ctx ) );
	hal->ip_ctx->hal = hal;
	hal->scratchpad = malloc( CNIP_HAL_SCRATCHPAD_TX_SIZE );
	//DOES NOT CALL cnip_init_ip!!!
	return 0;
}

uint8_t CNIP_IRAM cnip_hal_pop8( cnip_hal * hal )
{
	return (hal->incoming_cur < hal->incoming_end)?*(hal->incoming_cur++):0;
}

uint8_t * CNIP_IRAM cnip_hal_pop_ptr_and_advance( cnip_hal * hal, int size_to_pop )
{
	int left = hal->incoming_end - hal->incoming_cur;
	if( size_to_pop > left )
		return 0;
	uint8_t * ret = hal->incoming_cur;
	hal->incoming_cur += size_to_pop;
	return ret;
}

uint8_t * CNIP_IRAM cnip_hal_push_ptr_and_advance( cnip_hal * hal, int size_to_push )
{
	int left = hal->outgoing_end - hal->outgoing_cur;
	if( size_to_push > left )
		return 0;
	uint8_t * ret = hal->outgoing_cur;
	hal->outgoing_cur += size_to_push;
	return ret;
}

int cnip_hal_popstr( cnip_hal * hal, char * data, uint8_t maxlen )
{
	int readleft = hal->incoming_end - hal->incoming_cur;
	int i;
	int maxtoread = (readleft < maxlen)?readleft:maxlen;
	for( i = 0; i < maxtoread; i++ )
	{
		char c = data[i] = hal->incoming_cur[i];
		if( !c ) { hal->incoming_cur += i+1; return i; }
	}
	if( i == maxlen )
	{
		for( ; i < readleft; i++ )
		{
			if( hal->incoming_cur[i] == 0)
			{
				hal->incoming_cur += i+1;
				return -1;
			}
		}
	}

	if( i != maxlen )
	{
		//Eh we ran out of data, but we still have room in the buffer.  That's probably fine.
		data[i] = 0;
		return i;
	}

	// Ran out of len and did not find a 0.
	return -2;
}


//Raw, on-wire pops. (assuming already in read)
void CNIP_IRAM cnip_hal_popblob( cnip_hal * hal, uint8_t * data, uint16_t len )
{
	int readleft = hal->incoming_end - hal->incoming_cur;
	if( readleft < len ) len = readleft;
	memcpy( data, hal->incoming_cur, len );
	hal->incoming_cur += len;
}

uint16_t CNIP_IRAM cnip_hal_pop16( cnip_hal * hal )
{
	uint16_t ret = 0;
	if( hal->incoming_end - hal->incoming_cur > 1 )
	{
		ret = ( hal->incoming_cur[0] << 8 ) | hal->incoming_cur[1];
		hal->incoming_cur+=2;
	}
	return ret;
}

uint32_t CNIP_IRAM cnip_hal_pop32LE( cnip_hal * hal )
{
	uint32_t ret = 0;
	if( hal->incoming_end - hal->incoming_cur > 3 )
	{
		ret = ( ((uint32_t*)hal->incoming_cur)[0] );
		hal->incoming_cur+=4;
	}
	return ret;
}


uint32_t CNIP_IRAM cnip_hal_pop32BE( cnip_hal * hal )
{
	uint32_t ret = 0;
	if( hal->incoming_end - hal->incoming_cur > 3 )
	{
		ret = be32toh( ((uint32_t*)hal->incoming_cur)[0] );
		hal->incoming_cur+=4;
	}
	return ret;
}



void CNIP_IRAM cnip_hal_pushstr( cnip_hal * hal, const char * msg )
{
	uint8_t * target = hal->outgoing_cur;
	uint8_t * end = hal->outgoing_end;
	while( target < end && (*(target) = *(msg++)) ) target++;
	hal->outgoing_cur = target;
}

void CNIP_IRAM cnip_hal_pushblob( cnip_hal * hal, const uint8_t * data, uint16_t len )
{
	int maxlen = hal->outgoing_end - hal->outgoing_cur;
	if( len > maxlen ) len = maxlen;
	memcpy( hal->outgoing_cur, data, len );
	hal->outgoing_cur += len;
}

void CNIP_IRAM cnip_hal_push32LE( cnip_hal * hal, uint32_t v )
{
	if( hal->outgoing_end - hal->outgoing_cur >= 4 )
	{
		hal->outgoing_cur[0] = v;
		hal->outgoing_cur[1] = v>>8;
		hal->outgoing_cur[2] = v>>16;
		hal->outgoing_cur[3] = v>>24;
		hal->outgoing_cur += 4;
	}

}


void CNIP_IRAM cnip_hal_push32BE( cnip_hal * hal, uint32_t v )
{
	if( hal->outgoing_end - hal->outgoing_cur >= 4 )
	{
		hal->outgoing_cur[0] = v>>24;
		hal->outgoing_cur[1] = v>>16;
		hal->outgoing_cur[2] = v>>8;
		hal->outgoing_cur[3] = v;
		hal->outgoing_cur += 4;
	}
}


void CNIP_IRAM cnip_hal_push16( cnip_hal * hal, uint16_t v )
{
	if( hal->outgoing_end - hal->outgoing_cur >= 2 )
	{
		hal->outgoing_cur[0] = v>>8;
		hal->outgoing_cur[1] = v & 0xff;
		hal->outgoing_cur += 2;
	}
}

//Start a new send.									//OPENING
void CNIP_IRAM cnip_hal_startsend( cnip_hal * hal, cnip_mem_address start, uint16_t length )
{
	uint8_t * sp = start;
	hal->outgoing_base = hal->outgoing_cur = sp;
	hal->outgoing_end = sp + length;
}

//Deselects the chip, can halt operation.			//CLOSURE
void CNIP_IRAM cnip_hal_stopop( cnip_hal * hal )
{
	//On this system, this is a NO-OP.
}

//Start a checksum
uint16_t CNIP_IRAM cnip_hal_checksum( cnip_hal * hal, uint16_t offset, int16_t len )
{
	uint32_t csum = 0;

#ifdef ICMP_PROFILE_CHECKSUMS
	packet_stopwatch = GetCycleCount();
	int olen = len;
#endif

	//Xtensa-optimized checksum
	/*
		For 1024-byte packets 3430 cycles. ~1.5x faster.
	*/
	uint32_t * val = (uint32_t*)&hal->outgoing_base[offset];

	int odd = 0;

	//We have to do this odd frontloading because we need to make sure the operations are aligned.
	if( len && (((uint32_t)val) & 1) )
	{
		odd = 1;
		csum = *((uint8_t*)val);
		val = (uint32_t*)(((uint8_t*)val)+1);
		len--;
	}

	if( len && ( ((uint32_t)val) & 2 ) )
	{
		csum = *(uint16_t*)val;
		val = (uint32_t*)(((uint8_t*)val)+2);
		len -= 2;
	}

	for( ; len >= 4 ; len-=4 )
	{
		uint32_t csv = *val;
		val ++;
		csum += csv & 0xffff;
		csum += csv >> 16;
	}
	if( len >= 2 )	//Handle a possible 2-byte trailer
	{
		csum += *((uint16_t*)val);
		val = (uint32_t*)(((uint8_t*)val)+2);
		len -= 2;
	}
	if( len )
	{
		csum += *(uint8_t*)val;
	}

	uint16_t csd16;
	while( ( csd16 = csum>>16 ) )
	{
		csum = (csum & 0xffff) + csd16;
	}

	//If odd, need to flip bits... but we're already flipping once.
	if (!odd)
		csum = (csum>>8) | ((csum&0xff)<<8);	//Flip endianmess.

#ifdef ICMP_PROFILE_CHECKSUMS
	printf( "*CSOUT  %d %04x %p (%d) %d\n", GetCycleCount()-packet_stopwatch, ~csum, &hal->outgoing_base[offset], offset, olen );
#endif
	return (~csum)&0xffff;
}

void CNIP_IRAM cnip_hal_alter_word( cnip_hal * hal, uint16_t offset, uint16_t val )
{
	uint8_t * output = &hal->outgoing_base[offset];
	output[0] = val>>8;
	output[1] = val&0xff;
}


void CNIP_IRAM cnip_hal_make_output_from_input( cnip_hal * hal )
{
	hal->outgoing_base = hal->incoming_base;
	hal->outgoing_cur = hal->incoming_base;
	hal->outgoing_end = hal->incoming_end;
}

uint16_t CNIP_IRAM cnip_hal_get_write_length( cnip_hal * hal )
{
	return hal->outgoing_cur - hal->outgoing_base;
}

void CNIP_IRAM cnip_hal_endsend( cnip_hal * hal )
{
	cnip_hal_xmitpacket( hal, hal->outgoing_base, hal->outgoing_cur - hal->outgoing_base );
}

void CNIP_IRAM cnip_force_packet_length( cnip_hal * hal, uint16_t len )
{
	hal->outgoing_cur = hal->outgoing_base + len;
}

