###################################################################################
#                                                                                 #
# NAME: meson.build                                                               #
#                                                                                 #
# AUTHOR: Boaz Segev.                                                             #
#                                                                                 #
# WRITTEN BY: Michael Brockus.                                                    #
#                                                                                 #
# License: MIT                                                                    #
#                                                                                 #
###################################################################################



project('facil.io', 'c',
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


openssl = dependency('openssl', required: cc.get_id() != 'msvc')

if not openssl.found()
        libcrypto = cc.find_library('libcrypto', required: false)
        libssl = cc.find_library('libssl')
        openssl = [libcrypto, libssl]
endif
threads_dep = dependency('threads')
m_dep = cc.find_library('m', required : false)

subdir('lib')


facil_lib = library(meson.project_name(), 
    sources: facil_src,
    include_directories: facil_dir,
    dependencies: [ threads_dep, m_dep ],
    install: true)


facil_dep = declare_dependency(
    link_with: facil_lib,
    include_directories: facil_dir,
    dependencies: [ threads_dep, m_dep ])
