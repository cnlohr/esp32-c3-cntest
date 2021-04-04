#ifndef _CNIP_COMPAT_H
#define _CNIP_COMPAT_H

//IP device abstraction layer
/*
	For some device architectures, the Ethernet RAM is kept in a special
	place that cannot be accessed directly or would be slow to access 
	directly, so we can use these functions to scyphon data in and out.
	This is also ideal for situations where the Ethernet RAM is shared.
	For instance on an ATMega32, or an ATTiny85. 

	This particular implementation is written with a higher-level mindset.
	Geared more for processors like an STM32F407, or ESP32.
*/

#include <stdint.h>
#include "cnip_config.h"

struct cnip_ctx_t;

struct cnip_hal_t
{
	uint8_t mac[6];
	void * user;

	void * host; //For the next level down.

	uint8_t * incoming_base;
	uint8_t * incoming_end;
	uint8_t * incoming_cur;

	uint8_t * scratchpad;

	uint8_t * outgoing_base;
	uint8_t * outgoing_end;
	uint8_t * outgoing_cur;

	struct cnip_ctx_t * ip_ctx;

	void * host_lock;
};

typedef struct cnip_hal_t cnip_hal;

//This must be implemented by the next level down.
//return 0 if OK, otherwise nonzero.
//Packets are stored anywhere in the cnip_hal's memory.
//Packets expect to be in the following format:
// [dest mac x6] [protocol (80 00)] [data]
int8_t CNIP_IRAM cnip_hal_xmitpacket( cnip_hal * hal, cnip_mem_address start, uint16_t len );

//Called by underlying IP stack, after incoming_* has been updated.
void CNIP_IRAM cnip_hal_receivecallback( cnip_hal * hal );

inline int16_t CNIP_IRAM cnip_hal_bytesleft( cnip_hal * hal ) { return hal->incoming_end - hal->incoming_cur; }

//return 0 if OK, otherwise nonzero.
//Does not call cnip_init_ip
int8_t CNIP_IRAM cnip_hal_init( cnip_hal * hal, const unsigned char * macaddy );

//Finish up any reading. 							//CLOSURE (normally nop'd, except on systems with external MACs)
#define cnip_hal_finish_callback_now(hal)

//Raw, on-wire pops. (assuming already in read)
void CNIP_IRAM cnip_hal_popblob( cnip_hal * hal, uint8_t * data, uint16_t len );
inline void CNIP_IRAM cnip_hal_dumpbytes( cnip_hal * hal, uint8_t len ) { hal->incoming_cur += len; }

inline cnip_mem_address CNIP_IRAM cnip_hal_get_scratchpad( cnip_hal * hal ) { return hal->scratchpad; } 

uint16_t CNIP_IRAM cnip_hal_pop16( cnip_hal * hal );
uint32_t CNIP_IRAM cnip_hal_pop32BE( cnip_hal * hal );
uint32_t CNIP_IRAM cnip_hal_pop32LE( cnip_hal * hal );
uint8_t CNIP_IRAM cnip_hal_pop8( cnip_hal * hal );

uint8_t * CNIP_IRAM cnip_hal_pop_ptr_and_advance( cnip_hal * hal, int size_to_pop );

//Returns a null terminated string, even if there isn't enough room it forces the null termination.
//Also returns length of string.
int cnip_hal_popstr( cnip_hal * hal, char * data, uint8_t maxlen );


//Raw, on-wire push. (assuming already in write)
void CNIP_IRAM cnip_hal_pushblob( cnip_hal * hal, const uint8_t * data, uint16_t len );
void CNIP_IRAM cnip_hal_pushstr( cnip_hal * hal, const char * msg );

//XXX Note: On this architecture, there is no penalty for reading from flash
//so, we can do the same thing.
#define cnip_hal_pushpgmstr   cnip_hal_pushstr
#define cnip_hal_pushpgmblob  cnip_hal_pushblob

//inline void CNIP_IRAM cnip_hal_push8( cnip_hal * hal, uint8_t x ) { if( hal->outgoing_cur < hal->outgoing_end ) * (hal->outgoing_cur++) = x; }
//inline void CNIP_IRAM cnip_hal_pushzeroes( cnip_hal * hal, uint8_t nrzeroes ) { while( nrzeroes-- ) cnip_hal_push8( hal, 0 ); }

#define cnip_hal_push8( hal, x ) { if( hal->outgoing_cur < hal->outgoing_end ) * (hal->outgoing_cur++) = x; }
#define cnip_hal_pushzeroes( hal, nrzeroes ) { int n = nrzeroes; while( n-- ) cnip_hal_push8( hal, 0 ); }

void CNIP_IRAM cnip_hal_push32BE( cnip_hal * hal, uint32_t v );
void CNIP_IRAM cnip_hal_push32LE( cnip_hal * hal, uint32_t v );
void CNIP_IRAM cnip_hal_push16( cnip_hal * hal, uint16_t v );

uint8_t * CNIP_IRAM cnip_hal_push_ptr_and_advance( cnip_hal * hal, int size_to_push );



//Start a new send.									//OPENING
void CNIP_IRAM cnip_hal_startsend( cnip_hal * hal, cnip_mem_address start, uint16_t length );
uint16_t CNIP_IRAM cnip_hal_get_write_length( cnip_hal * hal );

//End sending (calls xmitpacket with correct flags)	//CLOSURE
void CNIP_IRAM cnip_hal_endsend( cnip_hal * hal );

//Deselects the chip, can halt operation.			//CLOSURE
void CNIP_IRAM cnip_hal_stopop( cnip_hal * hal );

//Start a checksum
uint16_t CNIP_IRAM cnip_hal_checksum( cnip_hal * hal, uint16_t start_offset, int16_t len );

void CNIP_IRAM cnip_hal_alter_word( cnip_hal * hal, uint16_t address_offset, uint16_t val );

void CNIP_IRAM cnip_hal_make_output_from_input( cnip_hal * hal );
void CNIP_IRAM cnip_force_packet_length( cnip_hal * hal, uint16_t len );

#endif

