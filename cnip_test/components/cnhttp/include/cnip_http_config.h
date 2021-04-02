#ifndef _HTTP_ESP32_H
#define _HTTP_ESP32_H

#define ICACHE_FLASH_ATTR
#define HTTP_IRAM CNIP_IRAM
#define USE_RAM_MFS

#include <cnip_hal.h>
#include "cnip_sha1.h"


//XXX TODO: Revisit the way we're writing this out.

typedef   void* cnhttp_socket;
#define HTTP_CONNECTIONS 10

#endif

