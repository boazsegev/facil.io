###################################################################################
#                                                                                 #
# NAME: lib/meson.build                                                           #
#                                                                                 #
# AUTHOR: Boaz Segev.                                                             #
#                                                                                 #
# WRITTEN BY: Michael Brockus.                                                    #
#                                                                                 #
# License: MIT                                                                    #
#                                                                                 #
###################################################################################



subdir('facil')

facil_lib = library(meson.project_name(), 
    sources: facil_src,
    include_directories: facil_dir,
    dependencies: [ threads_dep, m_dep ],
    install: true)
