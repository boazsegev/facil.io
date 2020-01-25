project(facilio C)
cmake_minimum_required(VERSION 2.4)

find_package(Threads REQUIRED)

set(facilio_SOURCES
  lib/facil/fio.c
  lib/facil/tls/fio_tls_missing.c
  lib/facil/tls/fio_tls_openssl.c
  lib/facil/fiobj/fio_siphash.c
  lib/facil/fiobj/fiobj_ary.c
  lib/facil/fiobj/fiobj_data.c
  lib/facil/fiobj/fiobj_hash.c
  lib/facil/fiobj/fiobj_json.c
  lib/facil/fiobj/fiobj_mustache.c
  lib/facil/fiobj/fiobj_numbers.c
  lib/facil/fiobj/fiobj_str.c
  lib/facil/fiobj/fiobject.c
  lib/facil/cli/fio_cli.c
  lib/facil/http/http.c
  lib/facil/http/http1.c
  lib/facil/http/http_internal.c
  lib/facil/http/websockets.c
  lib/facil/http/parsers/http1_parser.c
  lib/facil/redis/redis_engine.c
)

add_library(facilio_objlib OBJECT ${facilio_SOURCES})
target_link_libraries(facilio_objlib
  PRIVATE Threads::Threads
  PUBLIC pthread
  PUBLIC m
  )
target_include_directories(facilio_objlib
  PUBLIC  lib
  PUBLIC  lib/facil
  PUBLIC  lib/facil/tls
  PUBLIC  lib/facil/fiobj
  PUBLIC  lib/facil/cli
  PUBLIC  lib/facil/http
  PUBLIC  lib/facil/http/parsers
  PUBLIC  lib/facil/redis
)

set_property(TARGET facilio_objlib PROPERTY POSITION_INDEPENDENT_CODE 1)
add_library(facilio_static STATIC $<TARGET_OBJECTS:facilio_objlib>)
add_library(facilio SHARED $<TARGET_OBJECTS:facilio_objlib>)
