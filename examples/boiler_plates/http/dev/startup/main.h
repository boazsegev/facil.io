#include "facil.h"

/* Global access to command line arguments */

extern const int ARGC;
extern char const **ARGV;

/* Parsed command line arguments */

extern char const *HTTP_PUBLIC_FOLDER;
extern unsigned long HTTP_BODY_LIMIT;
extern unsigned char HTTP_TIMEOUT;
extern unsigned long WEBSOCKET_MSG_LIMIT;
extern unsigned char WEBSOCKET_PING_INTERVAL;
extern const char *HTTP_PORT;
extern const char *HTTP_ADDRESS;

extern int VERBOSE;
