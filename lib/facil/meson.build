###################################################################################
#                                                                                 #
# NAME: lib/facil/meson.build                                                     #
#                                                                                 #
# AUTHOR: Boaz Segev.                                                             #
# WRITTEN BY: Michael Brockus.                                                    #
#                                                                                 #
# License: MIT                                                                    #
#                                                                                 #
###################################################################################



facil_src = [
    'fio.c',
    join_paths('tls', 'fio_tls_missing.c'),
    join_paths('tls', 'fio_tls_openssl.c'),
    join_paths('fiobj', 'fio_siphash.c'),
    join_paths('fiobj', 'fiobj_ary.c'),
    join_paths('fiobj', 'fiobj_data.c'),
    join_paths('fiobj', 'fiobj_hash.c'),
    join_paths('fiobj', 'fiobj_json.c'),
    join_paths('fiobj', 'fiobj_mustache.c'),
    join_paths('fiobj', 'fiobj_numbers.c'),
    join_paths('fiobj', 'fiobj_str.c'),
    join_paths('fiobj', 'fiobject.c'),
    join_paths('cli', 'fio_cli.c'),
    join_paths('http', 'http.c'),
    join_paths('http', 'http1.c'),
    join_paths('http', 'http_internal.c'),
    join_paths('http', 'websockets.c'),
    join_paths('http', 'parsers', 'http1_parser.c'),
    join_paths( 'redis', 'redis_engine.c')
]

facil_dir = include_directories(
    '.', 
    'tls', 
    'fiobj', 
    'cli', 
    'http', 
    'redis',
	join_paths('http', 'parsers'))

facil_lib = library(meson.project_name(), 
    sources: facil_src,
    dependencies: [ threads_dep, m_dep],
    include_directories: facil_dir)