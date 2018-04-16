### Output
# binary name and location
NAME=demo

ifndef DESTDIR
DESTDIR=tmp
endif

### Library folders
# the .c and .cpp source files root folder - subfolders are automatically included
LIB_ROOT=lib
# publicly used subfolders in the lib root
LIB_PUBLIC_SUBFOLDERS=facil/core facil/core/types facil/core/types/fiobj facil/services facil/http facil/http/parsers facil/redis
# privately used subfolders in the lib root (this distinction is for CMake)
LIB_PRIVATE_SUBFOLDERS= 

### Development folders
# The development, non-library .c file(s) (i.e., the one with `int main(void)`.
MAIN_ROOT=src
# Development subfolders in the main root
MAIN_SUBFOLDERS=

### Build Root
# temporary folder will be cleared out and deleted between fresh builds
# All object files will be placed in this folder
TMP_ROOT=tmp

### Compiler & Linker flags
# any librries required (only names, ommit the "-l" at the begining)
LINKER_LIBS=pthread m
LINKER_LIBS_EXT=
# optimization level.
OPTIMIZATION=-O2 -march=native
# Warnings... i.e. -Wpedantic -Weverything -Wno-format-pedantic
WARNINGS= -Wshadow -Wall -Wextra -Wno-missing-field-initializers -Wpedantic
# any extra include folders, space seperated list. (i.e. `pg_config --includedir`)
INCLUDE= ./
# any preprocessosr defined flags we want, space seperated list (i.e. DEBUG )
FLAGS:= 

### Helpers
# The library details for CMake incorporation. Can be safely removed.
CMAKE_LIBFILE_NAME=CMakeLists.txt
# dumps all library files in one place
DUMP_LIB=libdump

# add DEBUG flag if requested
ifdef DEBUG
  $(info * Detected DEBUG environment flag, enforcing debug mode compilation)
	FLAGS:=$(FLAGS) DEBUG
	# # comment the following line if you want to use a different address sanitizer or a profiling tool. 
	OPTIMIZATION:=-O0 -march=native -fsanitize=address -fno-omit-frame-pointer 
	# possibly useful:  -Wconversion -Wcomma -fsanitize=undefined -Wshadow
	# go crazy with clang: -Weverything -Wno-cast-qual -Wno-used-but-marked-unused -Wno-reserved-id-macro -Wno-padded -Wno-disabled-macro-expansion -Wno-documentation-unknown-command -Wno-bad-function-cast -Wno-missing-prototypes
else
	FLAGS:=$(FLAGS) NODEBUG
endif


# c compiler
ifndef CC
	CC=gcc
endif
# c++ compiler
ifndef CPP
	CPP=g++
endif

##############
## OS specific data - compiler, assembler etc.

ifneq ($(OS),Windows_NT)
	OS := $(shell uname)
else
	$(error *** Windows systems aren\'t supported by this makefile / library.)
endif
ifeq ($(OS),Darwin) # Run MacOS commands
	# debugger
	DB=lldb
	# disassemble tool. Use stub to disable.
	# DISAMS=otool -dtVGX
	# documentation commands
	# DOCUMENTATION=cldoc generate $(INCLUDE_STR) -- --output ./html $(foreach dir, $(LIB_PUBLIC_SUBFOLDERS), $(wildcard $(addsuffix /, $(basename $(dir)))*.h*))
else
	# debugger
	DB=gdb
	# disassemble tool, leave undefined.
	# DISAMS=otool -tVX
	DOCUMENTATION=
endif

#####################
# Auto computed values
BIN = $(DESTDIR)/$(NAME)

LIBDIR_PUB = $(LIB_ROOT) $(foreach dir, $(LIB_PUBLIC_SUBFOLDERS), $(addsuffix /,$(basename $(LIB_ROOT)))$(dir))
LIBDIR_PRIV = $(foreach dir, $(LIB_PRIVATE_SUBFOLDERS), $(addsuffix /,$(basename $(LIB_ROOT)))$(dir))

LIBDIR = $(LIBDIR_PUB) $(LIBDIR_PRIV)
LIBSRC = $(foreach dir, $(LIBDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.c*))

MAINDIR = $(MAIN_ROOT) $(foreach main_root, $(MAIN_ROOT) , $(foreach dir, $(MAIN_SUBFOLDERS), $(addsuffix /,$(basename $(main_root)))$(dir)))
MAINSRC = $(foreach dir, $(MAINDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.c*))

FOLDERS = $(LIBDIR) $(MAINDIR)
SOURCES = $(LIBSRC) $(MAINSRC)

BUILDTREE =$(foreach dir, $(FOLDERS), $(addsuffix /, $(basename $(TMP_ROOT)))$(basename $(dir)))

CCL = $(CC)

INCLUDE_STR = $(foreach dir,$(INCLUDE),$(addprefix -I, $(dir))) $(foreach dir,$(FOLDERS),$(addprefix -I, $(dir)))

MAIN_OBJS = $(foreach source, $(MAINSRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))
LIB_OBJS = $(foreach source, $(LIBSRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))
OBJS_DEPENDENCY:=$(LIB_OBJS:.o=.d) $(MAIN_OBJS:.o=.d)


# S2N TLS/SSL library: https://github.com/awslabs/s2n
ifeq ($(shell printf "\#include <s2n.h>\\n int main(void) {}" | $(CC) $(INCLUDE_STR) -ls2n -xc -o /dev/null - >> /dev/null 2> /dev/null ; echo $$? 2> /dev/null ), 0)
  $(info * Detected the s2n library, setting HAVE_S2N)
	FLAGS:=$(FLAGS) HAVE_S2N
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) s2n
endif

# add BearSSL/OpenSSL library flags
ifeq ($(shell printf "\#include <bearssl.h>\\n int main(void) {}" | $(CC) $(INCLUDE_STR) -lbearssl -xc -o /dev/null - >> /dev/null 2> /dev/null ; echo $$? ), 0)
  $(info * Detected the BearSSL library, setting HAVE_BEARSSL)
	FLAGS:=$(FLAGS) HAVE_BEARSSL
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) bearssl
else
ifeq ($(shell printf "\#include <openssl/ssl.h>\\nint main(void) {}" | $(CC) $(INCLUDE_STR) -lcrypto -lssl -xc -o /dev/null - >> /dev/null 2> /dev/null ; echo $$? 2> /dev/null), 0)
  $(info * Detected the OpenSSL library, setting HAVE_OPENSSL)
	FLAGS:=$(FLAGS) HAVE_OPENSSL
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) crypto ssl
endif
endif

# add ZLib library flags
ifeq ($(shell printf "\#include <zlib.h>\\nint main(void) {}" | $(CC) $(INCLUDE_STR) -lz -xc -o /dev/null - >> /dev/null 2> /dev/null ; echo $$? ), 0)
  $(info * Detected the zlib library, setting HAVE_ZLIB)
	FLAGS:=$(FLAGS) HAVE_ZLIB
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) z
endif

# add PostgreSQL library flags
ifeq ($(shell printf "\#include <libpq-fe.h>\\nint main(void) {}\n" | $(CC) $(INCLUDE_STR) -lpg -xc -o /dev/null - >> /dev/null 2> /dev/null ; echo $$? ), 0)
  $(info * Detected the PostgreSQL library, setting HAVE_POSTGRESQL)
	FLAGS:=$(FLAGS) HAVE_POSTGRESQL
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) pg
endif


#####################
# Updated flags and final values

FLAGS_STR = $(foreach flag,$(FLAGS),$(addprefix -D, $(flag)))
CFLAGS:= $(CFLAGS) -g -std=c11 -fpic $(FLAGS_STR) $(WARNINGS) $(OPTIMIZATION) $(INCLUDE_STR)
CPPFLAGS:= $(CPPFLAGS) -std=gnu++11 -fpic  $(FLAGS_STR) $(WARNINGS) $(OPTIMIZATION) $(INCLUDE_STR)
LINKER_FLAGS=$(foreach lib,$(LINKER_LIBS),$(addprefix -l,$(lib))) $(foreach lib,$(LINKER_LIBS_EXT),$(addprefix -l,$(lib)))
CFALGS_DEPENDENCY=-MT $@ -MMD -MP

########
## Main Tasks

$(NAME): build

build: | create_tree build_objects

build_objects: $(LIB_OBJS) $(MAIN_OBJS)
	@$(CCL) -o $(BIN) $^ $(OPTIMIZATION) $(LINKER_FLAGS)
	@$(DOCUMENTATION)

lib: | create_tree lib_build

lib_build: $(LIB_OBJS)
	@$(CCL) -shared -o $(DESTDIR)/libfacil.so $^ $(OPTIMIZATION) $(LINKER_FLAGS)
	@$(DOCUMENTATION)


%.o : %.c

ifdef DISAMS
$(TMP_ROOT)/%.o: %.c $(TMP_ROOT)/%.d
	@$(CC) -c $< -o $@ $(CFALGS_DEPENDENCY) $(CFLAGS) 
	@$(DISAMS) $@ > $@.s 

$(TMP_ROOT)/%.o: %.cpp $(TMP_ROOT)/%.d
	@$(CPP) -o $@ -c $< $(CFALGS_DEPENDENCY) $(CPPFLAGS)
	$(eval CCL = $(CPP))
	@$(DISAMS) $@ > $@.s

$(TMP_ROOT)/%.o: %.c++ $(TMP_ROOT)/%.d
	@$(CPP) -o $@ -c $< $(CFALGS_DEPENDENCY) $(CPPFLAGS)
	$(eval CCL = $(CPP))
	@$(DISAMS) $@ > $@.s


else
$(TMP_ROOT)/%.o: %.c $(TMP_ROOT)/%.d
	@$(CC) -c $< -o $@ $(CFALGS_DEPENDENCY) $(CFLAGS) 

$(TMP_ROOT)/%.o: %.cpp $(TMP_ROOT)/%.d
	@$(CC) -c $< -o $@ $(CFALGS_DEPENDENCY) $(CFLAGS) 
	$(eval CCL = $(CPP))

$(TMP_ROOT)/%.o: %.c++ $(TMP_ROOT)/%.d
	@$(CC) -c $< -o $@ $(CFALGS_DEPENDENCY) $(CFLAGS) 
	$(eval CCL = $(CPP))
endif

$(TMP_ROOT)/%.d: ;

-include $(OBJS_DEPENDENCY)

.PHONY : test 
test: | clean 
	@DEBUG=1 $(MAKE) test_build_and_run
	-@rm $(BIN) 2> /dev/null
	-@rm -R $(TMP_ROOT) 2> /dev/null

.PHONY : test_optimized
test_optimized: | clean 
	@$(MAKE) test_build_and_run
	-@rm $(BIN) 2> /dev/null
	-@rm -R $(TMP_ROOT) 2> /dev/null

.PHONY : test_build_and_run
test_build_and_run: | create_tree test_add_flags test_build
	$(BIN)

.PHONY : test_add_flags
test_add_flags: 
	$(eval CFLAGS:=-coverage $(CFLAGS) -DDEBUG=1 -Werror)
	$(eval LINKER_FLAGS:=-coverage -DDEBUG=1 $(LINKER_FLAGS))

.PHONY : test_build
test_build: $(LIB_OBJS)
	@$(CC) -c ./tests/shorts.c -o $(TMP_ROOT)/shorts.o $(CFALGS_DEPENDENCY) $(CFLAGS) 
	@$(CCL) -o $(BIN) $(LIB_OBJS) $(TMP_ROOT)/shorts.o $(OPTIMIZATION) $(LINKER_FLAGS)

.PHONY : clean
clean:
	-@rm $(BIN) 2> /dev/null || echo "" >> /dev/null
	-@rm -R $(TMP_ROOT) 2> /dev/null || echo "" >> /dev/null
	-@mkdir -p $(BUILDTREE) 

.PHONY : run
run: | build
	@$(BIN)

.PHONY : db
db: | clean
	DEBUG=1 $(MAKE) build
	$(DB) $(BIN)


.PHONY : create_tree
create_tree:
	-@mkdir -p $(BUILDTREE) 2> /dev/null

########
## Helper Tasks

ifndef DUMP_LIB
.PHONY : libdump
libdump: cmake

else

ifeq ($(LIBDIR_PRIV),)

.PHONY : libdump
libdump: cmake
	-@rm -R $(DUMP_LIB) 2> /dev/null
	-@mkdir $(DUMP_LIB) 
	-@mkdir $(DUMP_LIB)/src 
	-@mkdir $(DUMP_LIB)/include 
	-@mkdir $(DUMP_LIB)/all  # except README.md files 
	-@cp -n $(foreach dir,$(LIBDIR_PUB), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^m]*)) $(DUMP_LIB)/all 2> /dev/null
	-@cp -n $(foreach dir,$(LIBDIR_PUB), $(wildcard $(addsuffix /, $(basename $(dir)))*.h*)) $(DUMP_LIB)/include 2> /dev/null
	-@cp -n $(foreach dir,$(LIBDIR_PUB), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^hm]*)) $(DUMP_LIB)/src 2> /dev/null	

else

.PHONY : libdump
libdump: cmake
	-@rm -R $(DUMP_LIB) 2> /dev/null
	-@mkdir $(DUMP_LIB) 
	-@mkdir $(DUMP_LIB)/src 
	-@mkdir $(DUMP_LIB)/include 
	-@mkdir $(DUMP_LIB)/all  # except README.md files 
	-@cp -n $(foreach dir,$(LIBDIR_PUB), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^m]*)) $(DUMP_LIB)/all 2> /dev/null
	-@cp -n $(foreach dir,$(LIBDIR_PUB), $(wildcard $(addsuffix /, $(basename $(dir)))*.h*)) $(DUMP_LIB)/include 2> /dev/null
	-@cp -n $(foreach dir,$(LIBDIR_PUB), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^hm]*)) $(DUMP_LIB)/src 2> /dev/null	
	-@cp -n $(foreach dir,$(LIBDIR_PRIV), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^m]*)) $(DUMP_LIB)/all 2> /dev/null
	-@cp -n $(foreach dir,$(LIBDIR_PRIV), $(wildcard $(addsuffix /, $(basename $(dir)))*.h*)) $(DUMP_LIB)/include 2> /dev/null
	-@cp -n $(foreach dir,$(LIBDIR_PRIV), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^hm]*)) $(DUMP_LIB)/src 2> /dev/null

endif
endif

ifndef CMAKE_LIBFILE_NAME
.PHONY : cmake
cmake:

else

.PHONY : cmake
cmake:
	-@rm $(CMAKE_LIBFILE_NAME) 2> /dev/null
	@touch $(CMAKE_LIBFILE_NAME)
	@echo 'project(facil.io C)' >> $(CMAKE_LIBFILE_NAME)
	@echo 'cmake_minimum_required(VERSION 2.4)' >> $(CMAKE_LIBFILE_NAME)
	@echo '' >> $(CMAKE_LIBFILE_NAME)
	@echo 'find_package(Threads REQUIRED)' >> $(CMAKE_LIBFILE_NAME)
	@echo '' >> $(CMAKE_LIBFILE_NAME)
	@echo 'set(facil.io_SOURCES' >> $(CMAKE_LIBFILE_NAME)
	@$(foreach src,$(LIBSRC),echo '  $(src)' >> $(CMAKE_LIBFILE_NAME);)
	@echo ')' >> $(CMAKE_LIBFILE_NAME)
	@echo '' >> $(CMAKE_LIBFILE_NAME)
	@echo 'add_library(facil.io $${facil.io_SOURCES})' >> $(CMAKE_LIBFILE_NAME)
	@echo 'target_link_libraries(facil.io' >> $(CMAKE_LIBFILE_NAME)
	@echo '  PRIVATE Threads::Threads' >> $(CMAKE_LIBFILE_NAME)
	@$(foreach src,$(LINKER_LIBS),echo '  PUBLIC $(src)' >> $(CMAKE_LIBFILE_NAME);)
	@echo '  )' >> $(CMAKE_LIBFILE_NAME)
	@echo 'target_include_directories(facil.io' >> $(CMAKE_LIBFILE_NAME)
	@$(foreach src,$(LIBDIR_PUB),echo '  PUBLIC  $(src)' >> $(CMAKE_LIBFILE_NAME);)
	@$(foreach src,$(LIBDIR_PRIV),echo '  PRIVATE $(src)' >> $(CMAKE_LIBFILE_NAME);)
	@echo ')' >> $(CMAKE_LIBFILE_NAME)
	@echo '' >> $(CMAKE_LIBFILE_NAME)

endif

# Prints the make variables, used for debugging the makefile
.PHONY : vars
vars:
	@echo "CC: $(CC)"
	@echo ""
	@echo "BIN: $(BIN)"
	@echo ""
	@echo "LIBDIR_PUB: $(LIBDIR_PUB)"
	@echo ""
	@echo "LIBDIR_PRIV: $(LIBDIR_PRIV)"
	@echo ""
	@echo "MAINDIR: $(MAINDIR)"
	@echo ""
	@echo "FOLDERS: $(FOLDERS)"
	@echo ""
	@echo "BUILDTREE: $(BUILDTREE)"
	@echo ""
	@echo "LIBSRC: $(LIBSRC)"
	@echo ""
	@echo "MAINSRC: $(MAINSRC)"
	@echo ""
	@echo "LIB_OBJS: $(LIB_OBJS)"
	@echo ""
	@echo "MAIN_OBJS: $(MAIN_OBJS)"
	@echo ""
	@echo "OBJS_DEPENDENCY: $(OBJS_DEPENDENCY)"
	@echo ""
	@echo "CFLAGS: $(CFLAGS)"
	@echo ""
	@echo "CPPFLAGS: $(CPPFLAGS)"
	@echo ""
	@echo "LINKER_LIBS: $(LINKER_LIBS)"
	@echo ""
	@echo "LINKER_FLAGS: $(LINKER_FLAGS)"


