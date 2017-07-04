/*
Edit this file to call any network services initialization functions you might
use.

The default implementation simply initiates the HTTP implementation.
*/
#include "websockets.h"

/* implemented in the http_service.c file */
void initialize_http_service(void);

void setup_network_services(void) { initialize_http_service(); }
