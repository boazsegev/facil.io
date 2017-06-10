project(facil.io C)

find_package(Threads REQUIRED)

set(facil.io_SOURCES
  ./lib/core/defer.c
  ./lib/core/evio.c
  ./lib/core/facil.c
  ./lib/core/sock.c
  ./lib/core/types/fio_dict.c
  ./lib/services/pubsub.c
  ./lib/http/http.c
  ./lib/http/http1.c
  ./lib/http/http1_request.c
  ./lib/http/http1_response.c
  ./lib/http/http1_simple_parser.c
  ./lib/http/http_request.c
  ./lib/http/http_response.c
  ./lib/http/websockets.c
  ./lib/bscrypt/bscrypt/base64.c
  ./lib/bscrypt/bscrypt/hex.c
  ./lib/bscrypt/bscrypt/misc.c
  ./lib/bscrypt/bscrypt/random.c
  ./lib/bscrypt/bscrypt/sha1.c
  ./lib/bscrypt/bscrypt/sha2.c
  ./lib/bscrypt/bscrypt/siphash.c
  ./lib/bscrypt/bscrypt/xor-crypt.c
)

add_library(facil.io ${facil.io_SOURCES})
target_link_libraries(facil.io PRIVATE Threads::Threads)
target_include_directories(facil.io
  PUBLIC  ./lib
  PUBLIC  ./lib/core
  PUBLIC  ./lib/core/types
  PUBLIC  ./lib/services
  PUBLIC  ./lib/http
  PRIVATE ./lib/bscrypt
  PRIVATE ./lib/bscrypt/bscrypt
)

