#include "cnip_mdns.h"
#include "cnip_core.h"
#include <string.h>
#include <stdint.h>

#ifdef DISABLE_MDNS
int JoinGropMDNS() { }
void SetupMDNS() { }
void AddMDNSService( const char * ServiceName, const char * Text, int port ) { }
void AddMDNSName( const char * ToDup ) { }
void ClearMDNSNames() { }

#else

#define MDNS_BRD 0xfb0000e0


static char * MDNSNames[MAX_MDNS_NAMES];
//static uint8_t igmp_bound[4];
static char MyLocalName[MAX_MDNS_PATH+7];

static int nr_services = 0;
static char * MDNSServices[MAX_MDNS_SERVICES];
static char * MDNSServiceTexts[MAX_MDNS_SERVICES];
static uint16_t MDNSServicePorts[MAX_MDNS_SERVICES];

//static char MDNSSearchName[MAX_MDNS_PATH];

static char * strdupcaselower( const char * src )
{
	int i;
	int len = strlen( src );
	char * ret = (char*)malloc( len+1 );
	for( i = 0; i < len+1; i++ )
	{
		if( src[i] >= 'A' && src[i] <= 'Z' )
			ret[i] = src[i] - 'A' + 'a';
		else
			ret[i] = src[i];
	}
	return ret;
}


void AddMDNSService( const char * ServiceName, const char * Text, int port )
{
	int i;
	for( i = 0; i < MAX_MDNS_SERVICES; i++ )
	{
		if( !MDNSServices[i] )
		{
			MDNSServices[i] = strdupcaselower( ServiceName );
			MDNSServiceTexts[i] = strdup( Text );
			MDNSServicePorts[i] = port;
			nr_services++;
			break;
		}
	}
}

void AddMDNSName( const char * ToDup )
{
	int i;
	for( i = 1; i < MAX_MDNS_NAMES; i++ )
	{
		if( !MDNSNames[i] )
		{
			if( i == 0 )
			{
				int sl = strlen( ToDup );
				memcpy( MyLocalName, ToDup, sl );
				memcpy( MyLocalName + sl, ".local", 7 );
			}
			MDNSNames[i] = strdupcaselower( ToDup );
			break;
		}
	}
}

void ClearMDNSNames()
{
	int i;
	for( i = 0; i < MAX_MDNS_NAMES; i++ )
	{
		if( MDNSNames[i] )
		{
			free( MDNSNames[i] );
			MDNSNames[i] = 0;
		}
	}
	for( i = 0; i < MAX_MDNS_SERVICES; i++ )
	{
		if( MDNSServices[i] )
		{
			free( MDNSServices[i] );			MDNSServices[i] = 0;
			free( MDNSServiceTexts[i] );		MDNSServiceTexts[i] = 0;
			MDNSServicePorts[i] = 0;
		}
	}
	nr_services = 0;
	memset( MyLocalName, 0, sizeof( MyLocalName ) );
	MDNSNames[0] = strdupcaselower( "esp82xx" );
}

uint8_t * ParseMDNSPath( uint8_t * dat, char * topop, int * len )
{
	int l;
	int j;
	*len = 0;
	do
	{
		l = *(dat++);
		if( l == 0 )
		{
			*topop = 0;
			return dat;
		}
		if( *len + l >= MAX_MDNS_PATH ) return 0;

		//If not our first time through, add a '.'
		if( *len != 0 )
		{
			*(topop++) = '.';
			(*len)++;
		}

		for( j = 0; j < l; j++ )
		{
			if( dat[j] >= 'A' && dat[j] <= 'Z' )
				topop[j] = dat[j] - 'A' + 'a';
			else
				topop[j] = dat[j];
		}
		topop += l;
		dat += l;
		*topop = 0; //Null terminate.
		*len += l;
	} while( 1 );
}

uint8_t * SendPathSegment( uint8_t * dat, const char * path )
{
	char c;
	int i;
	do
	{
		const char * pathstart = path;
		int this_seg_length = 0;
		while( (c = *(path++)) )
		{
			if( c == '.' ) break;
			this_seg_length++;
		}
		if( !this_seg_length ) return dat;

		*(dat++) = this_seg_length;
		for( i = 0; i < this_seg_length; i++ )
		{
			*(dat++) = *(pathstart++);
		}
	} while( c != 0 );
	return dat;
}

uint8_t * TackTemp( uint8_t * obptr )
{
	*(obptr++) = strlen( MDNSNames[0] );
	memcpy( obptr, MDNSNames[0], strlen( MDNSNames[0] ) );
	obptr+=strlen( MDNSNames[0] );
	*(obptr++) = 0xc0; *(obptr++) = 0x0c; //continue the name.
	return obptr;
}

static void SendOurARecord( cnip_ctx * ctx, uint8_t * namestartptr, int xactionid, int stlen )
{
	//Found match with us.
	//Send a response.

	uint8_t outbuff[MAX_MDNS_PATH+32];
	uint8_t * obptr = outbuff;
	uint16_t * obb = (uint16_t*)outbuff;
	*(obb++) = xactionid;
	*(obb++) = CNIP_HTONS(0x8400); //Authortative response.
	*(obb++) = 0;
	*(obb++) = CNIP_HTONS(1); //1 answer.
	*(obb++) = 0;
	*(obb++) = 0;

	obptr = (uint8_t*)obb;
	memcpy( obptr, namestartptr, stlen+1 ); //Hack: Copy the name in.
	obptr += stlen+1;
	*(obptr++) = 0;
	*(obptr++) = 0x00; *(obptr++) = 0x01; //A record
	*(obptr++) = 0x80; *(obptr++) = 0x01; //Flush cache + in ptr.
	*(obptr++) = 0x00; *(obptr++) = 0x00; //TTL
	*(obptr++) = 0x00; *(obptr++) = 120;   //very short. (120 seconds)
	*(obptr++) = 0x00; *(obptr++) = 0x04; //Size 4 (IP)
	memcpy( obptr, &ctx->ip_addr, 4 );
	obptr+=4;

	cnip_switch_to_broadcast( ctx );
	cnip_util_emit_udp_packet( ctx, outbuff, obptr - outbuff, MDNS_BRD, 5353, 5353 );
}

static void SendAvailableServices( cnip_ctx * ctx, uint8_t * namestartptr, int xactionid, int stlen )
{
	printf( "SEND AVAILABLE\n" );
	int i;
	if( nr_services == 0 ) return;
	uint8_t outbuff[(MAX_MDNS_PATH+14)*MAX_MDNS_SERVICES+32];
	uint8_t * obptr = outbuff;
	uint16_t * obb = (uint16_t*)outbuff;
	*(obb++) = xactionid;
	*(obb++) = CNIP_HTONS(0x8400); //Authortative response.
	*(obb++) = 0;
	*(obb++) = CNIP_HTONS(nr_services); //Answers
	*(obb++) = 0;
	*(obb++) = 0;

	obptr = (uint8_t*)obb;

	for( i = 0; i < nr_services; i++ )
	{
		char localservicename[MAX_MDNS_PATH+8];

		memcpy( obptr, namestartptr, stlen+1 ); //Hack: Copy the name in.
		obptr += stlen+1;
		*(obptr++) = 0;
		*(obptr++) = 0x00; *(obptr++) = 0x0c; //PTR record
		*(obptr++) = 0x00; *(obptr++) = 0x01; //Don't flush cache + ptr
		*(obptr++) = 0x00; *(obptr++) = 0x00; //TTL
		*(obptr++) = 0x00; *(obptr++) = 0xf0; //4 minute TTL.

		int sl = strlen( MDNSServices[i] );
		memcpy( localservicename, MDNSServices[i], sl );
		memcpy( localservicename+sl, ".local", 7 );
		*(obptr++) = 0x00; *(obptr++) = sl+8;
		obptr = SendPathSegment( obptr, localservicename );
		*(obptr++) = 0x00;
	}

	//Send to broadcast.
	cnip_switch_to_broadcast( ctx );
	cnip_util_emit_udp_packet( ctx, outbuff, obptr - outbuff, MDNS_BRD, 5353, 5353 );
}



static void SendSpecificService( cnip_ctx * ctx, int sid, uint8_t*namestartptr, int xactionid, int stlen, int is_prefix )
{
	printf( "SEND SendSpecificService\n" );

	uint8_t outbuff[256];
	uint8_t * obptr = outbuff;
	uint16_t * obb = (uint16_t*)outbuff;

	const char *sn = MDNSServices[sid];
	const char *st = MDNSServiceTexts[sid]; int stl = strlen( st );
	int sp = MDNSServicePorts[sid];

	*(obb++) = xactionid;
	*(obb++) = CNIP_HTONS(0x8400); //We are authoratative. And this is a response.
	*(obb++) = 0;
	*(obb++) = CNIP_HTONS(sp?4:3); //4 answers. (or 3 if we don't have a port)
	*(obb++) = 0;
	*(obb++) = 0;


	if( !sn || !st ) return;

	obptr = (uint8_t*)obb;
//	memcpy( obptr, namestartptr, stlen+1 ); //Hack: Copy the name in.
//	obptr += stlen+1;
	obptr = SendPathSegment( obptr,  sn );
	obptr = SendPathSegment( obptr,  "local" );
	*(obptr++) = 0;

	*(obptr++) = 0x00; *(obptr++) = 0x0c; //PTR record
	*(obptr++) = 0x00; *(obptr++) = 0x01; //No Flush cache + in ptr.
	*(obptr++) = 0x00; *(obptr++) = 0x00; //TTL
	*(obptr++) = 0x00; *(obptr++) = 100; //very short. (100 seconds)

	if( !is_prefix )
	{
		*(obptr++) = 0x00; *(obptr++) = strlen( MDNSNames[0] ) + 3; //Service...
		obptr = TackTemp( obptr );
	}
	else
	{
		*(obptr++) = 0x00; *(obptr++) = 2;
		*(obptr++) = 0xc0; *(obptr++) = 0x0c; //continue the name.
	}


	if( !is_prefix )
	{
		obptr = TackTemp( obptr );
	}
	else
	{
		*(obptr++) = 0xc0; *(obptr++) = 0x0c; //continue the name.
	}

	*(obptr++) = 0x00; *(obptr++) = 0x10; //TXT record
	*(obptr++) = 0x80; *(obptr++) = 0x01; //Flush cache + in ptr.
	*(obptr++) = 0x00; *(obptr++) = 0x00; //TTL
	*(obptr++) = 0x00; *(obptr++) = 100; //very short. (100 seconds)
	*(obptr++) = 0x00; *(obptr++) = stl + 1; //Service...
	*(obptr++) = stl; //Service length
	memcpy( obptr, st, stl );
	obptr += stl;

	//Service record
	if( sp )
	{
		int localnamelen = strlen( MyLocalName );


		if( !is_prefix )
		{
			obptr = TackTemp( obptr );
		}
		else
		{
			*(obptr++) = 0xc0; *(obptr++) = 0x0c; //continue the name.
		}

		//obptr = TackTemp( obptr );
		//*(obptr++) = 0xc0; *(obptr++) = 0x2f; //continue the name.
		*(obptr++) = 0x00; *(obptr++) = 0x21; //SRV record
		*(obptr++) = 0x80; *(obptr++) = 0x01; //Don't Flush cache + in ptr.
		*(obptr++) = 0x00; *(obptr++) = 0x00; //TTL
		*(obptr++) = 0x00; *(obptr++) = 100; //very short. (100 seconds)
		*(obptr++) = 0x00; *(obptr++) = localnamelen + 7 + 1; //Service?
		*(obptr++) = 0x00; *(obptr++) = 0x00; //Priority
		*(obptr++) = 0x00; *(obptr++) = 0x00; //Weight
		*(obptr++) = sp>>8; *(obptr++) = sp&0xff; //Port#
		obptr = SendPathSegment( obptr, MyLocalName );
		*(obptr++) = 0;
	}

	//A Record
	obptr = SendPathSegment( obptr, MyLocalName );
	*(obptr++) = 0;
	*(obptr++) = 0x00; *(obptr++) = 0x01; //A record
	*(obptr++) = 0x80; *(obptr++) = 0x01; //Flush cache + in ptr.
	*(obptr++) = 0x00; *(obptr++) = 0x00; //TTL
	*(obptr++) = 0x00; *(obptr++) = 30; //very short. (30 seconds)
	*(obptr++) = 0x00; *(obptr++) = 0x04; //Size 4 (IP)
	memcpy( obptr, &ctx->ip_addr, 4 );
	obptr+=4;

	//Send to broadcast.
	cnip_switch_to_broadcast( ctx );
	cnip_util_emit_udp_packet( ctx, outbuff, obptr - outbuff, MDNS_BRD, 5353, 5353 );
}


void got_mdns_packet(cnip_ctx * ctx, unsigned short len)
{
	int i, stlen;
	char path[MAX_MDNS_PATH];

	cnip_hal * hal = ctx->hal;
	//Pop off the UDP checksum.
	cnip_hal_pop16( hal );
	uint16_t xactionid = cnip_hal_pop16( hal );
	uint16_t flags = cnip_hal_pop16( hal );
	uint16_t questions = cnip_hal_pop16( hal );
	/*uint16_t answers = */cnip_hal_pop16( hal );
	/*uint16_t authority_rrs = */ cnip_hal_pop16( hal );
	/*uint16_t additional_rrs =*/ cnip_hal_pop16( hal );

	uint8_t dataptr_buffer[len-12];
	uint8_t * dataptr = dataptr_buffer;
	cnip_hal_popblob( hal, dataptr, len-12 );
	uint8_t * dataend = dataptr + len-12;

	if( flags & 0x8000 )
	{
		//Response

		//Unused; MDNS does not fit the browse model we want to use.
	}
	else
	{
		//Query
		for( i = 0; i < questions; i++ )
		{
			uint8_t * namestartptr = dataptr;
			//Work our way through.
			dataptr = ParseMDNSPath( dataptr, path, &stlen );

			if( dataend - dataptr < 10 ) return;

			if( !dataptr )
			{
				return;
			}

			int pathlen = strlen( path );
			if( pathlen < 6 )
			{
				continue;
			}
			if( strcmp( path + pathlen - 6, ".local" ) != 0 )
			{
				continue;
			}
			uint16_t record_type = ( dataptr[0] << 8 ) | dataptr[1];
			/*uint16_t record_class = ( dataptr[2] << 8 ) | dataptr[3]; */

			const char * path_first_dot = path;
			const char * cpp = path;
			while( *cpp && *cpp != '.' ) cpp++;
			int dotlen = 0;
			if( *cpp == '.' )
			{
				path_first_dot = cpp+1;
				dotlen = path_first_dot - path - 1;
			}
			else
				path_first_dot = 0;

			int found = 0;
			for( i = 0; i < MAX_MDNS_NAMES; i++ )
			{
				//Handle [hostname].local, or [hostname].[service].local
				if( MDNSNames[i] && dotlen && strncmp( MDNSNames[i], path, dotlen ) == 0 && dotlen == strlen( MDNSNames[i] ))
				{
					found = 1;
					if( record_type == 0x0001 ) //A Name Lookup.
						SendOurARecord( ctx, namestartptr, xactionid, stlen );
					else
						SendSpecificService( ctx, i, namestartptr, xactionid, stlen, 1 );
				}
			}

			if( !found ) //Not a specific entry lookup...
			{
				//Is this a browse?
				if( strcmp( path, "_services._dns-sd._udp.local" ) == 0 )
				{
					SendAvailableServices( ctx, namestartptr, xactionid, stlen );
				}
				else
				{
					//A specific service?
					for( i = 0; i < MAX_MDNS_SERVICES; i++ )
					{
						const char * srv = MDNSServices[i];
						if( !srv ) continue;
						int sl = strlen( srv );
						if( strncmp( path, srv, sl ) == 0 )
						{
							SendSpecificService( ctx, i, namestartptr, xactionid, stlen, 0 );
						}
					}
				}
			}
		}
	}
}

void SetupMDNS()
{
	MDNSNames[0] = strdupcaselower( "todo_device_name" );
}

void cnip_mdns_tick( cnip_ctx * ctx )
{
	static uint8_t mdns_timeout;
	if( ctx->ip_addr )
	{
		if( mdns_timeout == 0 )
		{
			cnip_switch_to_broadcast( ctx );
			cnip_hal * hal = ctx->hal;
			cnip_hal_stopop( hal );
			cnip_hal_startsend( hal, cnip_hal_get_scratchpad( hal ), CNIP_HAL_SCRATCHPAD_TX_SIZE ); 
			cnip_send_etherlink_header( ctx, 0x0800 );
			cnip_send_ip_header( ctx, 28, MDNS_BRD, 2 );
			cnip_hal_push8( hal, 0x16 ); //Membership Query
			cnip_hal_push8( hal, 0xfe );
			cnip_hal_push16( hal, 0x0000 ); //checksum
			cnip_hal_push32LE( hal, MDNS_BRD );
			cnip_hal_stopop( hal );
			cnip_hal_alter_word( hal, 18+6, cnip_hal_checksum( hal, 8+6, 20 ) ); //IP Header checksum
			cnip_hal_alter_word( hal, 18+18, cnip_hal_checksum( hal, 18+16, 8 ) ); //IGMP checksum
			cnip_hal_endsend( hal );
			mdns_timeout = 100;
		}
		else
		{
			mdns_timeout--;
		}
	}
}


#endif


