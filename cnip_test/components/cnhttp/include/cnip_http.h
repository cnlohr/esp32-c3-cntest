#ifndef _CNHTTP_H
#define _CNHTTP_H

#include "cnip_http_config.h"
#include <stdint.h>
#include "mfs.h"

extern struct HTTPConnection * curhttp;

//You must provide this.
void CNIP_IRAM HTTPCustomStart( );
void CNIP_IRAM HTTPCustomCallback( );  //called when we can send more data
void CNIP_IRAM WebSocketData( char * data, int len );
void CNIP_IRAM WebSocketTick( );
void CNIP_IRAM WebSocketNew();
void CNIP_IRAM HTTPHandleInternalCallback( );
uint8_t hex2byte( const char * c );
void CNIP_IRAM NewWebSocket();
void CNIP_IRAM et_espconn_disconnect( cnhttp_socket socket );

//Internal Functions
void CNIP_IRAM HTTPTick( uint8_t timedtick ); 
int CNIP_IRAM URLDecode( char * decodeinto, int maxlen, const char * buf );
void CNIP_IRAM WebSocketGotData( char * data, int size );
void CNIP_IRAM WebSocketTickInternal();
void CNIP_IRAM WebSocketSend( uint8_t * data, int size );

//Host-level functions
void CNIP_IRAM my_base64_encode(const unsigned char *data, unsigned int input_length, uint8_t * encoded_data );
void CNIP_IRAM Uint32To10Str( char * out, uint32_t dat );
void CNIP_IRAM http_recvcb(int whichhttp, char *pusrdata, unsigned short length);
void CNIP_IRAM http_disconnetcb(int whichhttp);
int  CNIP_IRAM httpserver_connectcb( cnhttp_socket socket ); // return which HTTP it is.  -1 for failure
void CNIP_IRAM DataStartPacket();

void CNIP_IRAM PushString( const char * data );
void CNIP_IRAM PushByte( uint8_t c );
void CNIP_IRAM PushBlob( const uint8_t * datam, int len );
uint8_t * CNIP_IRAM GetPtrToPushAndAdvance( int len );
int CNIP_IRAM TCPCanSend( cnhttp_socket socket, int size );
int CNIP_IRAM TCPDoneSend( cnhttp_socket socket );
int CNIP_IRAM EndTCPWrite( cnhttp_socket socket );

#ifndef HTTP_CONNECTIONS
#define HTTP_CONNECTIONS 50
#endif

#ifndef MAX_HTTP_PATHLEN
#define MAX_HTTP_PATHLEN 80
#endif
#define HTTP_SERVER_TIMEOUT		500


#define HTTP_STATE_NONE        0
#define HTTP_STATE_WAIT_METHOD 1
#define HTTP_STATE_WAIT_PATH   2
#define HTTP_STATE_WAIT_PROTO  3

#define HTTP_STATE_WAIT_FLAG   4
#define HTTP_STATE_WAIT_INFLAG 5
#define HTTP_STATE_DATA_XFER   7
#define HTTP_STATE_DATA_WEBSOCKET   8

#define HTTP_WAIT_CLOSE        15

struct HTTPConnection
{
	uint8_t  state:4;
	uint8_t  state_deets;
	uint8_t  is_dynamic:1;

	//Provides path, i.e. "/index.html" but, for websockets, the last 
	//32 bytes of the buffer are used for the websockets key.  
	uint8_t  pathbuffer[MAX_HTTP_PATHLEN];
	uint16_t timeout;

	union data_t
	{
		struct MFSFileInfo filedescriptor;
		struct UserData { uint16_t a, b, c; } user;
		struct UserDataPtr { void * v; } userptr;
	} data;

	void * rcb;
	void * rcbDat; //For websockets primarily.
	void * ccb;                          //Close callback (used for websockets, primarily)

	uint32_t bytesleft;
	uint32_t bytessofar;

	uint8_t  is404:1;
	uint8_t  isdone:1;
	uint8_t  isfirst:1;
	uint8_t  keep_alive:1;
	uint8_t  need_resend:1;
	uint8_t  send_pending:1; //If we can send data, we should?

	cnhttp_socket socket;
};

extern struct HTTPConnection HTTPConnections[HTTP_CONNECTIONS];

#endif

