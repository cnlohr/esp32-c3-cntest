//Copyright 2012 <>< Charles Lohr, under the MIT-x11 or NewBSD license.  You choose.

#ifndef TCP_H
#define TCP_H

#include <stdint.h>
#include "cnip_config.h"

#ifdef INCLUDE_TCP

#define TCP_PROTOCOL_NUMBER 6
#define TCP_HEADER_LENGTH 20
#define TCP_AND_HEADER_SIZE (34 - 20)

//General processes:
//
//1: Request a connection to a foreign host.
//   a. Get a free socket.
//   b. Flag it as SYN_SENT
//   c. load local_port, dest_port, dest_addr with values.
//   d. load seq_num with a random value.
//   e. load ack_num will already be filled with 0.
//   f. set sendtype to be 0x01

//TODO:
//   1) Add TCP Client support (May never be done)
//   2) TCP Keep Alive (we already have idle)
//   3) There is still an error somewhere that happens when a packet is dropped.

#define TCP_HAS_SERVER

typedef enum {
	CLOSED = 0,
	ESTABLISHED,
	CLOSING_WAIT,
	CLOSING_WAIT2,  //Currently unused.
} connection_state;


struct cnip_ctx_t;

//Size is 32 bytes (try to keep this as low as possible)
struct cnip_tcp_t
{
	connection_state state:3;

	uint16_t this_port;
	uint16_t dest_port;
	uint32_t dest_addr;
	uint32_t seq_num; //The values on our /send to foreign/ - use this when we receive an ack.
	uint32_t ack_num; //The values on our /receive from foreign/
	uint8_t  remote_mac[6];

	//For sending
	uint8_t time_since_sent; //if zero, then no packets are pending, if nonzero, it counts up.
	uint16_t idletime;
	uint8_t retries;
	uint8_t sendtype; //TCP Flags section

	//Send pointer?
	uint8_t sendptr[TCP_BUFFERSIZE];
	uint16_t sendlength;

	void * userv;
	int    useri;

	struct cnip_ctx_t * ctx;
};

typedef struct cnip_tcp_t cnip_tcp;

#include <cnip_core.h>

//USER MUST IMPLEMENT
//Must return a socket ID, or 0 if no action is to be taken, i.e. dump the socket.
uint8_t CNIP_IRAM cnip_tcp_recv_syn_cb( struct cnip_ctx_t * ctx, uint16_t portno );

//Return 1 if you DID emit a packet (with ack) Return 0 if you JUST received data and we need to ack and close out the packet.
uint8_t CNIP_IRAM cnip_tcp_recv_data_cb( cnip_tcp * tcp, int socketno, uint16_t totallen );

//User must provide this for being called back
//This is called right before the socket /must/ be closed.  Like if the remote host sent us a RST
void CNIP_IRAM   cnip_tcp_connection_closing_cb( cnip_tcp * tcp, int socketno );

//User must provide this for being called back
//This is called right before the socket /must/ be closed.  Like if the remote host sent us a RST
void CNIP_IRAM   cnip_received_tcp_ack_cb( cnip_tcp * tcp, int socketno );



//REGULAR FUNCTIONS


void CNIP_IRAM cnip_tcp_init( struct cnip_ctx_t * ctx );

//Call this at a regular tick interval.
void CNIP_IRAM cnip_tcp_tick( struct cnip_ctx_t * ctx );


//Finish current packet, but don't xmit.  This handles size and checksums.
void CNIP_IRAM cnip_finish_tcp_packet( cnip_tcp * tcp, uint16_t length );

//user must call this 100x/second
void CNIP_IRAM cnip_tick_tcp( struct cnip_ctx_t * ctx ); 

//user must init stack.
void CNIP_IRAM cnip_init_tcp( cnip_tcp * tcp );


//Rest of API
void CNIP_IRAM cnip_tcp_handle( struct cnip_ctx_t * ctx, uint16_t iptotallen);
void CNIP_IRAM cnip_tcp_request_closure( cnip_tcp * tcp );
void CNIP_IRAM cnip_tcp_request_connect( cnip_tcp * tcp );
uint8_t CNIP_IRAM cnip_tcp_can_send( cnip_tcp * tcp );

//always make sure 	cnip_hal_finish_callback_now(); is already called. 
//Also, make sure waiting_for_ack is not set in socket before updating data.  This does not check for that.
//After calling this you can push packets.

//Initialize a TCP socket write
void CNIP_IRAM cnip_tcp_start_write( cnip_tcp * tcp );
//End a TCP socket write. This computes checksum, length, etc. information
void CNIP_IRAM cnip_tcp_end_write( cnip_tcp * tcp );
//this emits a stored and written TCP datagram
void CNIP_IRAM cnip_tcp_emit( cnip_tcp * tcp );
//Utility, can call this on a syn to get a socket.
int8_t CNIP_IRAM cnip_tcp_get_free_connection( struct cnip_ctx_t * ctx );

#define ACKBIT (1<<4)
#define PSHBIT (1<<3)
#define RSTBIT (1<<2)
#define SYNBIT (1<<1)
#define FINBIT (1<<0)

#endif

#endif

