project(facil.io C)

find_package(Threads REQUIRED)

set(facil.io_SOURCES
  ./lib/facil/core/defer.c
  ./lib/facil/core/evio.c
  ./lib/facil/core/facil.c
  ./lib/facil/core/sock.c
  ./lib/facil/core/types/fio_dict.c
  ./lib/facil/services/fio_cli_helper.c
  ./lib/facil/services/pubsub.c
  ./lib/facil/http/http.c
  ./lib/facil/http/http1.c
  ./lib/facil/http/http1_parser.c
  ./lib/facil/http/http1_request.c
  ./lib/facil/http/http1_response.c
  ./lib/facil/http/http_request.c
  ./lib/facil/http/http_response.c
  ./lib/facil/http/websockets.c
  ./lib/facil/redis/fio2resp.c
  ./lib/facil/redis/redis_connection.c
  ./lib/facil/redis/redis_engine.c
  ./lib/facil/redis/resp.c
  ./lib/facil/bscrypt/bscrypt/base64.c
  ./lib/facil/bscrypt/bscrypt/hex.c
  ./lib/facil/bscrypt/bscrypt/misc.c
  ./lib/facil/bscrypt/bscrypt/random.c
  ./lib/facil/bscrypt/bscrypt/sha1.c
  ./lib/facil/bscrypt/bscrypt/sha2.c
  ./lib/facil/bscrypt/bscrypt/siphash.c
  ./lib/facil/bscrypt/bscrypt/xor-crypt.c
  ./lib/facil/core/types/fiobj/fiobj_alloc.c
  ./lib/facil/core/types/fiobj/fiobj_ary.c
  ./lib/facil/core/types/fiobj/fiobj_generic.c
  ./lib/facil/core/types/fiobj/fiobj_hash.c
  ./lib/facil/core/types/fiobj/fiobj_io.c
  ./lib/facil/core/types/fiobj/fiobj_json.c
  ./lib/facil/core/types/fiobj/fiobj_misc.c
  ./lib/facil/core/types/fiobj/fiobj_numbers.c
  ./lib/facil/core/types/fiobj/fiobj_primitives.c
  ./lib/facil/core/types/fiobj/fiobj_str.c
  ./lib/facil/core/types/fiobj/fiobj_sym.c
  ./lib/facil/core/types/fiobj/fiobj_tests.c
)

add_library(facil.io ${facil.io_SOURCES})
target_link_libraries(facil.io PRIVATE Threads::Threads)
target_link_libraries(facil.io m)
target_include_directories(facil.io
  PUBLIC  ./lib
  PUBLIC  ./lib/facil/core
  PUBLIC  ./lib/facil/core/types
  PUBLIC  ./lib/facil/services
  PUBLIC  ./lib/facil/http
  PUBLIC  ./lib/facil/redis
  PRIVATE ./lib/facil/bscrypt
  PRIVATE ./lib/facil/bscrypt/bscrypt
  PRIVATE ./lib/facil/core/types/fiobj
)

