#ifdef DEBUG
#include "hpack.h"
int main(void) { hpack_test(); }
#else
#define HPACK_BUILD_HPACK_STRUCT 1
#include "hpack.h"
#endif
