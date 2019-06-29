###################################################################################
#                                                                                 #
# NAME: meson.build                                                               #
#                                                                                 #
# AUTHOR: Boaz Segev.                                                             #
#                                                                                 #
# WRITTEN BY: Michael Brockus.                                                     #
#                                                                                 #
# CONTACT: <mailto:michael@squidfarts.com>                                        #
#                                                                                 #
# NOTICES:                                                                        #
#                                                                                 #
# MIT License                                                                     #
#                                                                                 #
# Copyright (c) Boaz Segev, 2018-2019                                             #
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



project('facil.io', 'c',
    version         : '0.7.2',
    license         : 'MIT',
    meson_version   : '>=0.50.0',
    default_options : 
    [ 
        'werror=true',
        'optimization=2', 
        'warning_level=3', 
        'b_sanitize=undefined', 
        'b_lto=true', 
        'b_lundef=true' 
    ]
)
cc = meson.get_compiler('c')
args_for_langs = 'c'



threads_dep = dependency('threads')
m_dep = cc.find_library('m', required : false)

subdir('lib')

facil_dir = include_directories(
    [
        join_paths('lib'),
        join_paths('lib', 'facil'),
        join_paths('lib', 'facil', 'tls'),
        join_paths('lib', 'facil', 'fiobj'),
        join_paths('lib', 'facil', 'cli'),
        join_paths('lib', 'facil', 'http'),
        join_paths('lib', 'facil', 'http', 'parsers'),
        join_paths('lib', 'facil', 'redis')
    ])


facil_lib = library(meson.project_name(), 
    sources: facil_src,
    include_directories: facil_dir,
    dependencies: [ threads_dep, m_dep ],
    install: true)


facil_dep = declare_dependency(
    link_with: facil_lib, 
    include_directories: facil_dir,
    dependencies: [ threads_dep, m_dep ])
