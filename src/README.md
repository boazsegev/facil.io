# Folder Hierarchy

This folder (`./src`) contains the main source files for the (C lang) Server Writing Tools project.

The `makefile` is designed for writing a new project and not for building a dynamic or static library. It simply compiles all the files in the source folders (./src * ./src/http) and links them.

Edit `tryeme.c` to test out code using Server Writing Tools, if you wish, either copy the project and use it as a boiler plate, or move the files you want to your own project. It's also possible to compile a dynamic/static library by edit the makefile.

I haven't tested the code for C++ compatibility, but I also feel there are wonderful native C++ alternatives.
