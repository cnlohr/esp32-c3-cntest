//Copyright 2012-2016 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or ColorChord License.  You Choose.

#include "cnip_http.h"
#include <string.h>
#include <stdio.h>

struct HTTPConnection HTTPConnections[HTTP_CONNECTIONS];

#define HTDEBUG( x... ) printf( x )
//#define HTDEBUG( x... )

//#define ISKEEPALIVE "keep-alive"
#define ISKEEPALIVE "close"

struct HTTPConnection * curhttp;

#define WS_KEY_LEN 36
#define WS_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_RETKEY_SIZEM1 32

#if	(WS_RETKEY_SIZE > MAX_HTTP_PATHLEN - 10 )
#error MAX_HTTP_PATHLEN too short.
#endif


void CloseEvent();
void InternalStartHTTP( );
void CNIP_IRAM HTTPHandleInternalCallback( );

void HTTPClose( int state_to_go_to )
{
	//This is dead code, but it is a testament to Charles.
	//Do not do this here.  Wait for the ESP to tell us the
	//socket is successfully closed.
	//curhttp->state = HTTP_STATE_NONE;
	curhttp->state = state_to_go_to;
	printf( "HTTPClose( %d )\n", state_to_go_to );
	et_espconn_disconnect( curhttp->socket ); 
	CloseEvent();
}


void CNIP_IRAM HTTPGotData( char * data, int length )
{
	uint8_t c;
	curhttp->timeout = 0;
	while( length-- )
	{
		c = *(data++);
		//printf( "%c (%02x) %d\n", c, c, curhttp->state );

		switch( curhttp->state )
		{
		case HTTP_STATE_WAIT_METHOD:
			if( c == ' ' )
			{
				curhttp->state = HTTP_STATE_WAIT_PATH;
				curhttp->state_deets = 0;
			}
			break;
		case HTTP_STATE_WAIT_PATH:
			curhttp->pathbuffer[curhttp->state_deets++] = c;
			if( curhttp->state_deets == MAX_HTTP_PATHLEN )
			{
				curhttp->state_deets--;
			}
			
			if( c == ' ' )
			{
				//Tricky: If we're a websocket, we need the whole header.
				curhttp->pathbuffer[curhttp->state_deets-1] = 0;
				curhttp->state_deets = 0;

				if( strncmp( (const char*)curhttp->pathbuffer, "/d/ws", 5 ) == 0 )
				{
					curhttp->state = HTTP_STATE_DATA_WEBSOCKET;
					curhttp->state_deets = 0;
				}
				else
				{
					curhttp->state = HTTP_STATE_WAIT_PROTO; 
				}
			}
			break;
		case HTTP_STATE_WAIT_PROTO:
			if( c == '\n' )
			{
				curhttp->state = HTTP_STATE_WAIT_FLAG;
			}
			break;
		case HTTP_STATE_WAIT_FLAG:
			if( c == '\n' )
			{
				curhttp->state = HTTP_STATE_DATA_XFER;
				InternalStartHTTP( );
			}
			else if( c != '\r' )
			{
				curhttp->state = HTTP_STATE_WAIT_INFLAG;
			}
			break;
		case HTTP_STATE_WAIT_INFLAG:
			if( c == '\n' )
			{
				curhttp->state = HTTP_STATE_WAIT_FLAG;
				curhttp->state_deets = 0;
			}
			break;
		case HTTP_STATE_DATA_XFER:
			//Ignore any further data?
			length = 0;
			break;
		case HTTP_STATE_DATA_WEBSOCKET:
			WebSocketGotData( data-1, length+1 );
			return;
		case HTTP_WAIT_CLOSE:
			if( curhttp->keep_alive )
			{
				curhttp->state = HTTP_STATE_WAIT_METHOD;
			}
			else
			{
				HTTPClose( 0 );
			}
			break;
		default:
			break;
		};
	}

}


static void CNIP_IRAM DoHTTP( uint8_t timed )
{
	switch( curhttp->state )
	{
	case HTTP_STATE_NONE: //do nothing if no state.
		curhttp->send_pending = 0;
		break;
	case HTTP_STATE_DATA_XFER:
		curhttp->send_pending = 1;
		if( TCPCanSend( curhttp->socket, 1300 ) ) //Was TCPDoneSend
		{
			if( curhttp->is_dynamic )
			{
				HTTPCustomCallback( );
			}
			else
			{
				HTTPHandleInternalCallback( );
			}
		}
		break;
	case HTTP_WAIT_CLOSE:
		curhttp->send_pending = 0;
		if( TCPDoneSend( curhttp->socket ) )	//DoneSend checks to see if everything's been closed out and happy.
		{
			if( curhttp->keep_alive )
			{
				curhttp->state = HTTP_STATE_WAIT_METHOD;
			}
			else
			{
				HTTPClose( 0 );
			}
		}
		break;
	case HTTP_STATE_DATA_WEBSOCKET:
		curhttp->send_pending = 0;
		if( TCPCanSend( curhttp->socket, 1300 ) ) //Was TCPDoneSend
		{
			WebSocketTickInternal();
		}
		break;
	default:
		if( timed )
		{
			if( curhttp->timeout++ > HTTP_SERVER_TIMEOUT )
			{
				HTTPClose( 0 );
			}
		}
	}
}

void CNIP_IRAM HTTPTick( uint8_t timed )
{
	uint8_t i;
	for( i = 0; i < HTTP_CONNECTIONS; i++ )
	{
		if( curhttp ) { HTDEBUG( "HTTPRXQ\n" ); break; }
		curhttp = &HTTPConnections[i];
		DoHTTP( timed );
		curhttp = 0;
	}
}

void CNIP_IRAM HTTPHandleInternalCallback( )
{
	uint16_t i;

	if( curhttp->isdone )
	{
		HTTPClose( 0 );//was HTTP_WAIT_CLOSE
		return;
	}
	if( curhttp->is404 )
	{
		DataStartPacket();
		PushString("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nFile not found.");
		EndTCPWrite( curhttp->socket );
		curhttp->isdone = 1;
		return;
	}
	if( curhttp->isfirst )
	{
		char stto[10];
		uint8_t slen = strlen( (const char*)curhttp->pathbuffer );
		const char * k;

		DataStartPacket();;
		//TODO: Content Length?  MIME-Type?
		PushString("HTTP/1.1 200 Ok\r\n");

		if( curhttp->bytesleft < 0xfffffffe )
		{
			PushString("Connection: "ISKEEPALIVE"\r\nContent-Length: ");
			Uint32To10Str( stto, curhttp->bytesleft );
			PushBlob( (uint8_t*)stto, strlen( stto ) );
			curhttp->keep_alive = 1;
		}
		else
		{
			PushString("Connection: close");
			curhttp->keep_alive = 0;
		}

		PushString( "\r\nContent-Type: " );
		//Content-Type?
		while( slen && ( curhttp->pathbuffer[--slen] != '.' ) );
		k = (const char*)&curhttp->pathbuffer[slen+1];

		if( strcmp( k, "mp3" ) == 0 )      PushString( "audio/mpeg3" );
		else if( strcmp( k, "jpg" ) == 0 ) PushString( "image/jpeg" );
		else if( strcmp( k, "png" ) == 0 ) PushString( "image/png" );
		else if( strcmp( k, "css" ) == 0 ) PushString( "text/css" );
		else if( strcmp( k, "js" ) == 0 )  PushString( "text/javascript" );
		else if( strcmp( k, "gz" ) == 0 )  PushString( "text/plain\r\nContent-Encoding: gzip\r\nCache-Control: public, max-age=3600" );			
		else if( curhttp->bytesleft == 0xfffffffe ) PushString( "text/plain" );
		else                               PushString( "text/html" );

		PushString( "\r\n\r\n" );
		EndTCPWrite( curhttp->socket );
		curhttp->isfirst = 0;

		return;
	}

	DataStartPacket();

	for( i = 0; i < 2 && curhttp->bytesleft; i++ )
	{
		int bpt = curhttp->bytesleft;
		if( bpt > MFS_SECTOR ) bpt = MFS_SECTOR;
		uint8_t * ptrpx = GetPtrToPushAndAdvance( bpt );
		curhttp->bytesleft = MFSReadSector( ptrpx, &curhttp->data.filedescriptor );
		//printf( "BYTES LEFT: %d\n", curhttp->bytesleft );
	}

	EndTCPWrite( curhttp->socket );

	if( !curhttp->bytesleft )
		curhttp->isdone = 1;
}

void CNIP_IRAM InternalStartHTTP( )
{
	int8_t i;
	char * path = (char*)&curhttp->pathbuffer[0];

	if( path[0] == '/' )
		path++;

	//printf( "InternalStartHTTP() -> %s\n", path );

	if( path[0] == 'd' && path[1] == '/' )
	{
		curhttp->is_dynamic = 1;
		curhttp->isfirst = 1;
		curhttp->isdone = 0;
		curhttp->is404 = 0;
		HTTPCustomStart();
		return;
	}

	if( !path[0] )
	{
		path = "index.html";
	}

	for( i = 0; path[i]; i++ )
		if( path[i] == '?' ) path[i] = 0;

	i = MFSOpenFile( path, &curhttp->data.filedescriptor );
	curhttp->bytessofar = 0;

	if( i < 0 )
	{
		HTDEBUG( "404(%s)\n", path );
		curhttp->is404 = 1;
		curhttp->isfirst = 1;
		curhttp->isdone = 0;
		curhttp->is_dynamic = 0;
	}
	else
	{
		curhttp->isfirst = 1;
		curhttp->isdone = 0;
		curhttp->is404 = 0;
		curhttp->is_dynamic = 0;
		curhttp->bytesleft = curhttp->data.filedescriptor.filelen;
	}

}


void CNIP_IRAM http_disconnetcb(int whichhttp )
{
	if( whichhttp >= 0 )
	{
		if( !HTTPConnections[whichhttp].is_dynamic ) MFSClose( &HTTPConnections[whichhttp].data.filedescriptor );
		HTTPConnections[whichhttp].state = 0;
	}
}


void CNIP_IRAM http_recvcb(int whichhttp, char *pusrdata, unsigned short length)
{
	//Though it might be possible for this to interrupt the other
	//tick task, I don't know if this is actually a probelem.
	//I'm adding this back-up-the-register just in case.
	if( curhttp ) { HTDEBUG( "Unexpected Race Condition\n" ); return; }

	curhttp = &HTTPConnections[whichhttp];
//	curdata = (uint8_t*)pusrdata;
//	curlen = length;
	HTTPGotData( pusrdata, length );
	curhttp = 0 ;

}

int CNIP_IRAM httpserver_connectcb( cnhttp_socket socket )
{
	int i;

	for( i = 0; i < HTTP_CONNECTIONS; i++ )
	{
		if( HTTPConnections[i].state == 0 )
		{
			//printf( "ConnectCB on %d\n", i );
			HTTPConnections[i].socket = socket;
			HTTPConnections[i].state = HTTP_STATE_WAIT_METHOD;
			break;
		}
	}
	if( i == HTTP_CONNECTIONS )
	{
		return -1;
	}

	return i;
}


int CNIP_IRAM URLDecode( char * decodeinto, int maxlen, const char * buf )
{
	int i = 0;

	for( ; buf && *buf; buf++ )
	{
		char c = *buf;
		if( c == '+' )
		{
			decodeinto[i++] = ' ';
		}
		else if( c == '?' || c == '&' )
		{
			break;
		}
		else if( c == '%' )
		{
			if( *(buf+1) && *(buf+2) )
			{
				decodeinto[i++] = hex2byte( buf+1 );
				buf += 2;
			}
		}
		else
		{
			decodeinto[i++] = c;
		}
		if( i >= maxlen -1 )  break;
		
	}
	decodeinto[i] = 0;
	return i;
}


#ifndef SHA1_HASH_LEN
#define SHA1_HASH_LEN SHA1_DIGEST_SIZE
#endif

void CNIP_IRAM WebSocketGotData( char * curdata, int curlen )
{
	while( curlen-- )
	{
		uint8_t c = *(curdata++);
		
		switch( curhttp->state_deets )
		{
		case 0:
		{
			int i = 0;
			char inkey[120];
			unsigned char hash[SHA1_HASH_LEN];
			SHA1_CTX c;

			curhttp->is_dynamic = 1;
			while( curlen > 20 )
			{
				curdata++; curlen--;
				if( strncmp( (char*)curdata, "Sec-WebSocket-Key: ", 19 ) == 0 )
				{
					break;
				}
			}

			if( curlen <= 21 )
			{
				HTDEBUG( "No websocket key found.\n" );
				curhttp->state = HTTP_WAIT_CLOSE;
				return;
			}

			curdata+= 19;
			curlen -= 19;

			while( curlen > 1 )
			{
				uint8_t lc = *(curdata++);
				inkey[i] = lc;
				curlen--;
				if( lc == '\r' )
				{
					inkey[i] = 0;
					break;
				}
				i++;
				if( i >= sizeof( inkey ) - WS_KEY_LEN - 5 )
				{
					HTDEBUG( "Websocket key too big.\n" );
					curhttp->state = HTTP_WAIT_CLOSE;
					return;
				}
			}
			if( curlen <= 1 )
			{
				HTDEBUG( "Invalid websocket key found.\n" );
				curhttp->state = HTTP_WAIT_CLOSE;
				return;
			}

			if( i + WS_KEY_LEN + 1 >= sizeof( inkey ) )
			{
				HTDEBUG( "WSKEY Too Big.\n" );
				curhttp->state = HTTP_WAIT_CLOSE;
				return;
			}

			memcpy( &inkey[i], WS_KEY, WS_KEY_LEN + 1 );
			i += WS_KEY_LEN;
			SHA1_Init( &c );
			SHA1_Update( &c, (uint8_t*)inkey, i );
			SHA1_Final( hash, &c );


			my_base64_encode( hash, SHA1_HASH_LEN, curhttp->pathbuffer + (MAX_HTTP_PATHLEN-WS_RETKEY_SIZEM1)  );

			curhttp->bytessofar = 0;
			curhttp->bytesleft = 0;

			NewWebSocket();
			//Respond...
			curhttp->state_deets = 1;
			break;
		}
		case 1:
			if( c == '\n' ) curhttp->state_deets = 2;
			break;
		case 2:
			if( c == '\r' ) curhttp->state_deets = 3;
			else curhttp->state_deets = 1;
			break;
		case 3:
			if( c == '\n' ) curhttp->state_deets = 4;
			else curhttp->state_deets = 1;
			break;
		case 5: //Established connection.
		{
			//XXX TODO: Seems to malfunction on large-ish packets.  I know it has problems with 140-byte payloads.

			if( curlen < 5 ) //Can't interpret packet.
				break;

			//uint8_t fin = c & 1;
			uint8_t opcode = c << 4;
			uint16_t payloadlen = *(curdata++);
			curlen--;
			if( !(payloadlen & 0x80) )
			{
				HTDEBUG( "Unmasked packet.\n" );
				curhttp->state = HTTP_WAIT_CLOSE;
				break;
			}

			if( opcode == 128 )
			{
				//Close connection.
				//HTDEBUG( "CLOSE\n" );
				//curhttp->state = HTTP_WAIT_CLOSE;
				//break;
			}

			payloadlen &= 0x7f;
			if( payloadlen == 127 )
			{
				//Very long payload.
				//Not supported.
				HTDEBUG( "Unsupported payload packet.\n" );
				curhttp->state = HTTP_WAIT_CLOSE;
				break;
			}
			else if( payloadlen == 126 )
			{
				payloadlen = (curdata[0] << 8) | curdata[1];
				curdata += 2;
				curlen -= 2;
			}
			uint32_t wsmask = ((uint32_t*)curdata)[0];
			curdata += 4;
			curlen -= 4;


			//XXX Warning: When packets get larger, they may split the
			//websockets packets into multiple parts.  We could handle this
			//but at the cost of prescious RAM.  I am chosing to just drop those
			//packets on the floor, and restarting the connection.
			if( curlen < payloadlen )
			{
				//extern int cork_binary_rx;
				//cork_binary_rx = 1;
				//HTDEBUG( "Websocket Fragmented. %d %d\n", curlen, payloadlen );
				//curhttp->state = HTTP_WAIT_CLOSE;
				HTDEBUG( "Websocket Fragmented. %d %d\n", curlen, payloadlen );
				curhttp->state = HTTP_WAIT_CLOSE;
				return;
			}

			//This section of code unmasks the packet.
			char * data_start = curdata;
			int i;
			//Get curdata 4-byte-aligned, we have to do a little weird stuff to rotate the mask in place.
			while( curlen && ((uint32_t)curdata & 3) )
			{
				char * mask = (char*)&wsmask;
				curdata[0] ^= mask[0];
				uint32_t msm;
#ifdef CNIP_LITTLE_ENDIAN
				msm = wsmask & 0xff;
				wsmask = wsmask >> 8;
				wsmask |= msm << 24;
#else
				msm = wsmask >> 24;
				wsmask = wsmask << 8;
				wsmask |= msm;
#endif
				curlen--;
				curdata++;
			}
			//We want to BLAST through this array and it should be 4-byte-aligned, as this should translate to:
			//l32, eor, s32, addi
			int curlend4 = curlen/4;
			for( i = 0; i < curlend4; i++ )
			{
				((uint32_t*)curdata)[0] ^= wsmask;
				curdata+=4;
			}
			curlen -= curlend4*4;
			//Cleanup any remaining bytes
			char * wsbmask = (char*)&wsmask;
			i = 0;
			while( i < curlen )
			{
				*(curdata++) ^= wsbmask[i++];
			}
			curlen = 0;


			//decrypt data in-place.

			WebSocketData( data_start, payloadlen );
			//curlen -= payloadlen; 
			//curdata += payloadlen;
			break;
		}
		default:
			break;
		}
	}
}

void CNIP_IRAM WebSocketTickInternal()
{
	switch( curhttp->state_deets )
	{
	case 4: //Has key full HTTP header, etc. wants response.
		DataStartPacket();;
		PushString( "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " );
		PushString( (char*)curhttp->pathbuffer + (MAX_HTTP_PATHLEN-WS_RETKEY_SIZEM1) );
		PushString( "\r\n\r\n" );
		EndTCPWrite( curhttp->socket );
		curhttp->state_deets = 5;
		curhttp->keep_alive = 0;
		break;
	case 5:
		WebSocketTick();
		break;
	}
}

void CNIP_IRAM WebSocketSend( uint8_t * data, int size )
{
	DataStartPacket();
	PushByte( 0x82 ); //0x81 is text.
	if( size >= 126 )
	{
		PushByte( 0x00 | 126 );
		PushByte( size>>8 );
		PushByte( size&0xff );
	}
	else
	{
		PushByte( 0x00 | size );
	}
	PushBlob( data, size );
	EndTCPWrite( curhttp->socket );
	curhttp->send_pending = 1;
}
/*
uint8_t CNIP_IRAM WSPOPMASK()
{
	uint8_t mask = wsmask[wsmaskplace];
	wsmaskplace = (wsmaskplace+1)&3;
	return (*curdata++)^(mask);
}
*/





