project(facil.io C)
cmake_minimum_required(VERSION 2.4)

find_package(Threads REQUIRED)

set(facil.io_SOURCES
  ./lib/facil/core/defer.c
  ./lib/facil/core/evio.c
  ./lib/facil/core/evio_callbacks.c
  ./lib/facil/core/evio_epoll.c
  ./lib/facil/core/evio_kqueue.c
  ./lib/facil/core/facil.c
  ./lib/facil/core/sock.c
  ./lib/facil/core/types/fiobj/fio_base64.c
  ./lib/facil/core/types/fiobj/fio_random.c
  ./lib/facil/core/types/fiobj/fio_sha1.c
  ./lib/facil/core/types/fiobj/fio_sha2.c
  ./lib/facil/core/types/fiobj/fiobj_ary.c
  ./lib/facil/core/types/fiobj/fiobj_hash.c
  ./lib/facil/core/types/fiobj/fiobj_internal.c
  ./lib/facil/core/types/fiobj/fiobj_io.c
  ./lib/facil/core/types/fiobj/fiobj_json.c
  ./lib/facil/core/types/fiobj/fiobj_numbers.c
  ./lib/facil/core/types/fiobj/fiobj_primitives.c
  ./lib/facil/core/types/fiobj/fiobj_str.c
  ./lib/facil/core/types/fiobj/fiobj_sym.c
  ./lib/facil/core/types/fiobj/fiobj_sym_hash.c
  ./lib/facil/core/types/fiobj/fiobj_tests.c
  ./lib/facil/core/types/fiobj/fiobject.c
  ./lib/facil/services/fio_cli_helper.c
  ./lib/facil/services/pubsub.c
  ./lib/facil/http/http.c
  ./lib/facil/http/http1.c
  ./lib/facil/http/http_internal.c
  ./lib/facil/http/websockets.c
  ./lib/facil/http/parsers/http1_parser.c
  ./lib/facil/redis/fio2resp.c
  ./lib/facil/redis/redis_connection.c
  ./lib/facil/redis/resp.c
)

add_library(facil.io ${facil.io_SOURCES})
target_link_libraries(facil.io
  PRIVATE Threads::Threads
  PUBLIC pthread
  PUBLIC m
  )
target_include_directories(facil.io
  PUBLIC  ./lib
  PUBLIC  ./lib/facil/core
  PUBLIC  ./lib/facil/core/types
  PUBLIC  ./lib/facil/core/types/fiobj
  PUBLIC  ./lib/facil/services
  PUBLIC  ./lib/facil/http
  PUBLIC  ./lib/facil/http/parsers
  PUBLIC  ./lib/facil/redis
)

