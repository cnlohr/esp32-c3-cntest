#ifndef CNIP_MDNS_H
#define CNIP_MDNS_H

#include <stdint.h>
#include "cnip_core.h"

#define MAX_MDNS_NAMES 5
#define MAX_MDNS_SERVICES 5
#define MAX_MDNS_PATH 32

void SetupMDNS();
int  JoinGropMDNS(); //returns nonzero on failure.

//This _does_ dup the data.  Don't worry about keeping the data around.
//Matching name, to respond with full suite of what-I-have
//The first name is automatically inserted to be the device name.
void AddMDNSName( const char * ToDup );

//Add service names here, I.e. _http._tcp.  or "esp8266" this will make us respond when we get
//those kinds of requests.
//
//All data is dupped
void AddMDNSService( const char * ServiceName, const char * Text, int port );

//Reset all services and matches.
void ClearMDNS();

uint8_t * ParseMDNSPath( uint8_t * dat, char * topop, int * len );
//Sends part of a path, but, does not terminate, so you an concatinate paths.
uint8_t * SendPathSegment( uint8_t * dat, const char * path );


//Called from stack
void got_mdns_packet(cnip_ctx * ctx, unsigned short len);

void cnip_mdns_tick( cnip_ctx * ctx );


#endif

