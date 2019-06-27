###################################################################################
#                                                                                 #
# NAME: meson.build                                                               #
#                                                                                 #
# AUTHOR: Michael Brockus.                                                        #
#                                                                                 #
# CONTACT: <mailto:michael@squidfarts.com>                                        #
#                                                                                 #
# NOTICES:                                                                        #
#                                                                                 #
# MIT License                                                                     #
#                                                                                 #
# Copyright (c) 2019 Team Squidfarts                                              #
#                                                                                 #
# Permission is hereby granted, free of charge, to any person obtaining a copy    #
# of this software and associated documentation files (the "Software"), to deal   #
# in the Software without restriction, including without limitation the rights    #
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell       #
# copies of the Software, and to permit persons to whom the Software is           #
# furnished to do so, subject to the following conditions:                        #
#                                                                                 #
# The above copyright notice and this permission notice shall be included in all  #
# copies or substantial portions of the Software.                                 #
#                                                                                 #
# DISCLAIMERS:                                                                    #
#                                                                                 #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR      #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,        #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE     #
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER          #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   #
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE   #
# SOFTWARE.                                                                       #
#                                                                                 #
###################################################################################



facil_src = files(
    [
        join_paths('fio.c'),
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
        join_paths( 'redis', 'redis_engine.c'),
    ])

facil_dir = include_directories(
    [
        '.', 
        'tls', 
        'fiobj', 
        'cli', 
        'http', 
        'redis',
        join_paths('http', 'parsers')
    ])