//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

#include "cnip_http.h"
#include <string.h>

CNIP_IRAM int HandleCommand( uint8_t * data, int len, uint8_t * backbuffer );


static CNIP_IRAM void huge()
{

	DataStartPacket();
	uint8_t * payload = GetPtrToPushAndAdvance( 512 );
	payload = payload;
	EndTCPWrite( curhttp->socket );
}


static CNIP_IRAM void echo()
{
	char mydat[128];
	int len = URLDecode( mydat, 128, (char*)curhttp->pathbuffer+8 );

	DataStartPacket();
	PushBlob( (uint8_t*)mydat, len );
	EndTCPWrite( curhttp->socket );

	curhttp->state = HTTP_WAIT_CLOSE;
}

static CNIP_IRAM void issue()
{
	static uint8_t backbuffer[1500];
	uint8_t  __attribute__ ((aligned (32))) buf[128];
	int len = URLDecode( (char*)buf, 128, (char*)curhttp->pathbuffer+9 );

	int r = HandleCommand( buf, len, backbuffer );

	if( r > 0 )
	{
		DataStartPacket();
		PushBlob( buf, r );
		EndTCPWrite( curhttp->socket );
	}
	curhttp->state = HTTP_WAIT_CLOSE;
}


void CNIP_IRAM HTTPCustomCallback( )
{
	if( curhttp->rcb )
		((void(*)())curhttp->rcb)();
	else
		curhttp->isdone = 1;
}


//Close of curhttp happened.
void CNIP_IRAM CloseEvent()
{
}

static void CNIP_IRAM WSEchoData( char* data, int len )
{
	WebSocketSend( (uint8_t*)data, len );
}



static void CNIP_IRAM WSCommandData( char * data, int len )
{
	static uint8_t backbuffer[1500];
	len = HandleCommand( (uint8_t*)data, len, (uint8_t*)backbuffer );
	WebSocketSend( (uint8_t*)backbuffer, len );
}



void CNIP_IRAM NewWebSocket()
{
	if( strcmp( (const char*)curhttp->pathbuffer, "/d/ws/echo" ) == 0 )
	{
		curhttp->rcb = 0;
		curhttp->rcbDat = (void*)&WSEchoData;
	}
	else if( strncmp( (const char*)curhttp->pathbuffer, "/d/ws/issue", 11 ) == 0 )
	{
		curhttp->rcb = 0;
		curhttp->rcbDat = (void*)&WSCommandData;
	}
	else
	{
		curhttp->is404 = 1;
	}
}




void CNIP_IRAM WebSocketTick()
{
	if( curhttp->rcb )
	{
		((void(*)())curhttp->rcb)();
	}
}

void CNIP_IRAM WebSocketData( char * data, int len )
{
	if( curhttp->rcbDat )
	{
		((void(*)( char *, int ))curhttp->rcbDat)( data,  len ); 
	}
}

void CNIP_IRAM HTTPCustomStart( )
{
	if( strncmp( (const char*)curhttp->pathbuffer, "/d/huge", 7 ) == 0 )
	{
		curhttp->rcb = (void(*)())&huge;
		curhttp->bytesleft = 0xffffffff;
	}
	else
	if( strncmp( (const char*)curhttp->pathbuffer, "/d/echo?", 8 ) == 0 )
	{
		curhttp->rcb = (void(*)())&echo;
		curhttp->bytesleft = 0xfffffffe;
	}
	else
	if( strncmp( (const char*)curhttp->pathbuffer, "/d/issue?", 9 ) == 0 )
	{
		curhttp->rcb = (void(*)())&issue;
		curhttp->bytesleft = 0xfffffffe;
	}
	else
	{
		curhttp->rcb = 0;
		curhttp->bytesleft = 0;
	}
	curhttp->isfirst = 1;
	HTTPHandleInternalCallback();
}


