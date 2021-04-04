#ifndef _CNIP_TFTP_H
#define _CNIP_TFTP_H

#include "cnip_core.h"


#define MAX_TFTP_CONNECTIONS 3
#define TFTP_LOCAL_BASE_PORT 6900

void HandleCNIPTFTPUDPPacket( cnip_ctx * ctx, int length );
void TickCNIP_TFTP();

#endif

