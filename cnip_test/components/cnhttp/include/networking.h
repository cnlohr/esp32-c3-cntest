#ifndef _NETWORKING_H
#define _NETWORKING_H

//These override the weak null functions in tcpip_adapter.
//You don't need to call them.
void InitNetworking();
void TickNetworking( int is_slow );

#endif

