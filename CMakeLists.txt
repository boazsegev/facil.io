project(facil.io C)

find_package(Threads REQUIRED)

set(facil.io_SOURCES
  src/services/pubsub.c
  src/http/http1_simple_parser.c
  src/http/http_request.c
  src/http/websockets.c
  src/http/http1_request.c
  src/http/http_response.c
  src/http/unused/hpack.c
  src/http/http.c
  src/http/http1.c
  src/http/http1_response.c
  src/bscrypt/bscrypt/sha1.c
  src/bscrypt/bscrypt/hex.c
  src/bscrypt/bscrypt/misc.c
  src/bscrypt/bscrypt/siphash.c
  src/bscrypt/bscrypt/random.c
  src/bscrypt/bscrypt/base64.c
  src/bscrypt/bscrypt/unused/mempool.c
  src/bscrypt/bscrypt/sha2.c
  src/bscrypt/bscrypt/xor-crypt.c
  src/core/defer.c
  src/core/facil.c
  src/core/sock.c
  src/core/evio.c
)

add_library(facil.io ${facil.io_SOURCES})
target_link_libraries(facil.io PRIVATE Threads::Threads)
target_include_directories(facil.io
  PUBLIC  src/http
  PRIVATE src/bscrypt
  PRIVATE src/bscrypt/bscrypt
  PUBLIC  src/core
  PRIVATE src/core/types
  PUBLIC  src/services
)
