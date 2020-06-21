#############################################################################
# This makefile was composed for facil.io
#
# Copyright (c) 2016-2019 Boaz Segev
# License MIT or ISC
#
# This makefile should be easilty portable.
#
# Should work on any POSIX system for any project.
#
#############################################################################

#############################################################################
# Compliation Output Settings
#############################################################################

# binary name and location
NAME?=fioapp

# a temporary folder that will be cleared out and deleted between fresh builds
# All object files will be placed in this folder
TMP_ROOT=tmp

# destination folder for the final compiled output
DEST?=$(TMP_ROOT)

# output folder for `make libdump` - dumps all library files (not source files) in one place.
DUMP_LIB=libdump

# The library details for CMake incorporation. Can be safely removed.
CMAKE_LIBFILE_NAME=CMakeLists.txt

#############################################################################
# Source Code Folder Settings
#############################################################################

# the main source .c and .cpp source files root folder
MAIN_ROOT=src
# subfolders under the main source root
MAIN_SUBFOLDERS=

#############################################################################
# Library Folder Settings
#############################################################################

# the library .c and .cpp source files root folder
LIB_ROOT=lib

# publicly used subfolders in the lib root
LIB_PUBLIC_SUBFOLDERS=facil facil/tls facil/fiobj facil/http facil/http/parsers facil/redis

# privately used subfolders in the lib root (this distinction is only relevant for CMake)
LIB_PRIVATE_SUBFOLDERS=


#############################################################################
# Testing code
#############################################################################

# Testing folder
TEST_ROOT=./tests
# Testing subfolders under the main testing root
TEST_SUBFOLDERS=
# Fill this in if the test folder contains more then a single file with a `main` function
TEST_SRC:=./tests/tests.c

#############################################################################
# Compiler / Linker Settings
#############################################################################

# any libraries required (only names, ommit the "-l" at the begining)
LINKER_LIBS=pthread m
# optimization level.
OPTIMIZATION=-O2 -march=native
# Warnings... i.e. -Wpedantic -Weverything -Wno-format-pedantic
WARNINGS=-Wshadow -Wall -Wextra -Wno-missing-field-initializers -Wpedantic
# any extra include folders, space seperated list. (i.e. `pg_config --includedir`)
INCLUDE=./
# any preprocessosr defined flags we want, space seperated list (i.e. DEBUG )
FLAGS:=
# c compiler
CC?=gcc
# c++ compiler
CXX?=g++
# C specific compiler options
C_EXTRA_OPT:=
# C++ specific compiler options
CXX_EXTRA_OPT:=-Wno-keyword-macro -Wno-c99-extensions -Wno-zero-length-array -Wno-variadic-macros
# c standard (if any, prefix using `-std=`)
CSTD?=-std=c11
# c++ standard (if any, prefix using `-std=`)
CXXSTD?=-std=c++11
# pkg-config
PKG_CONFIG?=pkg-config
# for internal use - don't change
LINKER_LIBS_EXT:=

#############################################################################
# Debug Settings
#############################################################################

# add DEBUG flag if requested
ifeq ($(DEBUG), 1)
  $(info * Detected DEBUG environment flag, enforcing debug mode compilation)
	FLAGS:=$(FLAGS) DEBUG
	# # comment the following line if you want to use a different address sanitizer or a profiling tool.
	OPTIMIZATION:=-O0 -march=native -fsanitize=address -fno-omit-frame-pointer
	# possibly useful:  -Wconversion -Wcomma -fsanitize=undefined -Wshadow
	# go crazy with clang: -Weverything -Wno-cast-qual -Wno-used-but-marked-unused -Wno-reserved-id-macro -Wno-padded -Wno-disabled-macro-expansion -Wno-documentation-unknown-command -Wno-bad-function-cast -Wno-missing-prototypes
else
	FLAGS:=$(FLAGS) NDEBUG NODEBUG
endif

#############################################################################
# Makefile Runtime Tests (sets flags, such as HAVE_OPENSSL)
#############################################################################

# Tests are performed unless the value is empty / missing

TEST4POLL:=1      # HAVE_KQUEUE / HAVE_EPOLL / HAVE_POLL
TEST4SOCKET:=1    # --- adds linker flags, not compilation flags
TEST4SSL:=1       # HAVE_OPENSSL / HAVE_BEARSSL + HAVE_S2N
TEST4SENDFILE:=1  # HAVE_SENDFILE
TEST4TM_ZONE:=1   # HAVE_TM_TM_ZONE
TEST4ZLIB:=       # HAVE_ZLIB
TEST4PG:=         # HAVE_POSTGRESQL
TEST4ENDIAN:=1    # __BIG_ENDIAN__=?

#############################################################################
# facil.io compilation flag helpers
#############################################################################

# add FIO_PUBSUB_SUPPORT flag if requested
ifdef FIO_PUBSUB_SUPPORT
	FLAGS:=$(FLAGS) FIO_PUBSUB_SUPPORT=$(FIO_PUBSUB_SUPPORT)
endif

#############################################################################
# OS Specific Settings (debugger, disassembler, etc')
#############################################################################


ifneq ($(OS),Windows_NT)
	OS:=$(shell uname)
else
	$(warning *** Windows systems might not work with this makefile / library.)
endif
ifeq ($(OS),Darwin) # Run MacOS commands
	# debugger
	DB=lldb
	# disassemble tool. Use stub to disable.
	# DISAMS=otool -dtVGX
	# documentation commands
	# DOCUMENTATION=cldoc generate $(INCLUDE_STR) -- --output ./html $(foreach dir, $(LIB_PUBLIC_SUBFOLDERS), $(wildcard $(addsuffix /, $(basename $(dir)))*.h*))
	# rule modifier (can't be indented)
$(DEST)/libfacil.so: LDFLAGS+=-dynamiclib -install_name $(realpath $(DEST))/libfacil.so
else
	# debugger
	DB=gdb
	# disassemble tool, leave undefined.
	# DISAMS=otool -tVX
	# documentation commands
	DOCUMENTATION=
endif

#############################################################################
# Automatic Setting Expansion
# (don't edit)
#############################################################################

BIN=$(DEST)/$(NAME)

LIBDIR_PUB=$(LIB_ROOT) $(foreach dir, $(LIB_PUBLIC_SUBFOLDERS), $(addsuffix /,$(basename $(LIB_ROOT)))$(dir))
LIBDIR_PRIV=$(foreach dir, $(LIB_PRIVATE_SUBFOLDERS), $(addsuffix /,$(basename $(LIB_ROOT)))$(dir))

LIBDIR=$(LIBDIR_PUB) $(LIBDIR_PRIV)
LIBSRC=$(foreach dir, $(LIBDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.c*))

MAINDIR=$(MAIN_ROOT) $(foreach main_root, $(MAIN_ROOT) , $(foreach dir, $(MAIN_SUBFOLDERS), $(addsuffix /,$(basename $(main_root)))$(dir)))
MAINSRC=$(foreach dir, $(MAINDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.c*))

TESTDIR=$(TEST_ROOT) $(foreach folder_root, $(TEST_ROOT) , $(foreach dir, $(TEST_SUBFOLDERS), $(addsuffix /,$(basename $(folder_root)))$(dir)))
ifeq (, $(TEST_SRC))
TEST_SRC=$(foreach dir, $(TESTDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.c*))
endif

FOLDERS=$(LIBDIR) $(MAINDIR) $(TESTDIR)
SOURCES=$(LIBSRC) $(MAINSRC)

BUILDTREE=$(foreach dir, $(FOLDERS), $(addsuffix /, $(basename $(TMP_ROOT)))$(basename $(dir)))

CCL=$(CC)

INCLUDE_STR=$(foreach dir,$(INCLUDE),$(addprefix -I, $(dir))) $(foreach dir,$(FOLDERS),$(addprefix -I, $(dir)))

MAIN_OBJS=$(foreach source, $(MAINSRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))
LIB_OBJS=$(foreach source, $(LIBSRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))
TEST_OBJS=$(foreach source, $(TEST_SRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))

OBJS_DEPENDENCY:=$(LIB_OBJS:.o=.d) $(MAIN_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

# TODO: fix code so this isn't required.
#
# GCC (at least) >= 7 triggers some bug when -fipa-icf is enabled
# (as in our default: -O2)
#
# Is there a hidden code issue in facil.io?
#
# https://kristerw.blogspot.com/2017/05/interprocedural-optimization-in-gcc.html
ifeq ($(shell $(CC) -v 2>&1 | grep -o "^gcc version"),gcc version)
	OPTIMIZATION+=-fno-ipa-icf
endif

#############################################################################
# TRY_COMPILE and TRY_COMPILE_AND_RUN functions
#
# Call using $(call TRY_COMPILE, code, compiler_flags)
#
# Returns shell code as string: "0" (success) or non-0 (failure)
#
# TRY_COMPILE_AND_RUN returns the program's shell code as string.
#############################################################################

TRY_COMPILE=$(shell printf $(1) | $(CC) $(INCLUDE_STR) $(LDFLAGS) $(2) -xc -o /dev/null - >> /dev/null 2> /dev/null ; echo $$? 2> /dev/null)
TRY_COMPILE_AND_RUN=$(shell printf $(1) | $(CC) $(2) -xc -o ./___fio_tmp_test_ - 2> /dev/null ; ./___fio_tmp_test_ >> /dev/null 2> /dev/null; echo $$?; rm ./___fio_tmp_test_ 2> /dev/null)
EMPTY:=

#############################################################################
# kqueue / epoll / poll Selection / Detection
# (no need to edit)
#############################################################################
ifdef TEST4POLL

FIO_POLL_TEST_KQUEUE:="\\n\
\#define _GNU_SOURCE\\n\
\#include <stdlib.h>\\n\
\#include <sys/event.h>\\n\
int main(void) {\\n\
	int fd = kqueue();\\n\
}\\n\
"

FIO_POLL_TEST_EPOLL:="\\n\
\#define _GNU_SOURCE\\n\
\#include <stdlib.h>\\n\
\#include <stdio.h>\\n\
\#include <sys/types.h>\\n\
\#include <sys/stat.h>\\n\
\#include <fcntl.h>\\n\
\#include <sys/epoll.h>\\n\
int main(void) {\\n\
	int fd = epoll_create1(EPOLL_CLOEXEC);\\n\
}\\n\
"

FIO_POLL_TEST_POLL:="\\n\
\#define _GNU_SOURCE\\n\
\#include <stdlib.h>\\n\
\#include <poll.h>\\n\
int main(void) {\\n\
  struct pollfd plist[18];\\n\
  memset(plist, 0, sizeof(plist[0]) * 18);\\n\
  poll(plist, 1, 1);\\n\
}\\n\
"

# Test for manual selection and then TRY_COMPILE with each polling engine
ifdef FIO_POLL
  $(info * Skipping polling tests, enforcing manual selection of: poll)
	FLAGS+=FIO_ENGINE_POLL HAVE_POLL
else ifdef FIO_FORCE_POLL
  $(info * Skipping polling tests, enforcing manual selection of: poll)
	FLAGS+=FIO_ENGINE_POLL HAVE_POLL
else ifdef FIO_FORCE_EPOLL
  $(info * Skipping polling tests, enforcing manual selection of: epoll)
	FLAGS+=FIO_ENGINE_EPOLL HAVE_EPOLL
else ifdef FIO_FORCE_KQUEUE
  $(info * Skipping polling tests, enforcing manual selection of: kqueue)
	FLAGS+=FIO_ENGINE_KQUEUE HAVE_KQUEUE
else ifeq ($(call TRY_COMPILE, $(FIO_POLL_TEST_EPOLL), $(EMPTY)), 0)
  $(info * Detected `epoll`)
	FLAGS+=HAVE_EPOLL
else ifeq ($(call TRY_COMPILE, $(FIO_POLL_TEST_KQUEUE), $(EMPTY)), 0)
  $(info * Detected `kqueue`)
	FLAGS+=HAVE_KQUEUE
else ifeq ($(call TRY_COMPILE, $(FIO_POLL_TEST_POLL), $(EMPTY)), 0)
  $(info * Detected `poll` - this is suboptimal fallback!)
	FLAGS+=HAVE_POLL
else
	$(warning No supported polling engine! won't be able to compile facil.io)
endif

endif # TEST4POLL
#############################################################################
# Detecting The `sendfile` System Call
# (no need to edit)
#############################################################################
ifdef TEST4SENDFILE

# Linux variation
FIO_SENDFILE_TEST_LINUX:="\\n\
\#define _GNU_SOURCE\\n\
\#include <stdlib.h>\\n\
\#include <stdio.h>\\n\
\#include <sys/sendfile.h>\\n\
int main(void) {\\n\
	off_t offset = 0;\\n\
	ssize_t result = sendfile(2, 1, (off_t *)&offset, 300);\\n\
}\\n\
"

# BSD variation
FIO_SENDFILE_TEST_BSD:="\\n\
\#define _GNU_SOURCE\\n\
\#include <stdlib.h>\\n\
\#include <stdio.h>\\n\
\#include <sys/types.h>\\n\
\#include <sys/socket.h>\\n\
\#include <sys/uio.h>\\n\
int main(void) {\\n\
	off_t sent = 0;\\n\
	off_t offset = 0;\\n\
	ssize_t result = sendfile(2, 1, offset, (size_t)sent, NULL, &sent, 0);\\n\
}\\n\
"

# Apple variation
FIO_SENDFILE_TEST_APPLE:="\\n\
\#define _GNU_SOURCE\\n\
\#include <stdlib.h>\\n\
\#include <stdio.h>\\n\
\#include <sys/types.h>\\n\
\#include <sys/socket.h>\\n\
\#include <sys/uio.h>\\n\
int main(void) {\\n\
	off_t sent = 0;\\n\
	off_t offset = 0;\\n\
	ssize_t result = sendfile(2, 1, offset, &sent, NULL, 0);\\n\
}\\n\
"

ifeq ($(call TRY_COMPILE, $(FIO_SENDFILE_TEST_LINUX), $(EMPTY)), 0)
  $(info * Detected `sendfile` (Linux))
	FLAGS+=USE_SENDFILE_LINUX HAVE_SENDFILE
else ifeq ($(call TRY_COMPILE, $(FIO_SENDFILE_TEST_BSD), $(EMPTY)), 0)
  $(info * Detected `sendfile` (BSD))
	FLAGS+=USE_SENDFILE_BSD HAVE_SENDFILE
else ifeq ($(call TRY_COMPILE, $(FIO_SENDFILE_TEST_APPLE), $(EMPTY)), 0)
  $(info * Detected `sendfile` (Apple))
	FLAGS+=USE_SENDFILE_APPLE HAVE_SENDFILE
else
  $(info * No `sendfile` support detected.)
	FLAGS:=$(FLAGS) USE_SENDFILE=0
endif

endif # TEST4SENDFILE
#############################################################################
# Detecting 'struct tm' fields
# (no need to edit)
#############################################################################
ifdef TEST4TM_ZONE

FIO_TEST_STRUCT_TM_TM_ZONE:="\\n\
\#define _GNU_SOURCE\\n\
\#include <time.h>\\n\
int main(void) {\\n\
	struct tm tm;\\n\
	tm.tm_zone = \"UTC\";\\n\
	return 0;\\n\
}\\n\
"

ifeq ($(call TRY_COMPILE, $(FIO_TEST_STRUCT_TM_TM_ZONE), $(EMPTY)), 0)
  $(info * Detected 'tm_zone' field in 'struct tm')
	FLAGS:=$(FLAGS) HAVE_TM_TM_ZONE=1
endif

endif # TEST4TM_ZONE
#############################################################################
# Detecting SystemV socket libraries
# (no need to edit)
#############################################################################
ifdef TEST4SOCKET

FIO_TEST_SOCKET_AND_NETWORK_SERVICE:="\\n\
\#include <sys/types.h>\\n\
\#include <sys/socket.h>\\n\
\#include <netinet/in.h>\\n\
\#include <arpa/inet.h>\\n\
int main(void) {\\n\
	struct sockaddr_in addr = { .sin_port = 0 };\\n\
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);\\n\
	if(fd == -1) return 1;\\n\
	if(inet_pton(AF_INET, \"127.0.0.1\", &addr.sin_addr) < 1) return 1;\\n\
	return connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0 ? 1 : 0;\\n\
}\\n\
"

ifeq ($(call TRY_COMPILE, $(FIO_TEST_SOCKET_AND_NETWORK_SERVICE), $(EMPTY)), 0)
  $(info * Detected native socket API, without additional libraries)
else ifeq ($(call TRY_COMPILE, $(FIO_TEST_SOCKET_AND_NETWORK_SERVICE), "-lsocket" "-lnsl"), 0)
  $(info * Detected socket API from libsocket and libnsl)
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) socket nsl
else
  $(error No socket API available)
endif

endif # TEST4SOCKET
#############################################################################
# SSL/ TLS Library Detection
# (no need to edit)
#############################################################################
ifdef TEST4SSL

# BearSSL requirement C application code
# (source code variation)
FIO_TLS_TEST_BEARSSL_SOURCE:="\\n\
\#define _GNU_SOURCE\\n\
\#include <stdlib.h>\\n\
\#include <bearssl.h>\\n\
int main(void) {\\n\
}\\n\
"

# BearSSL requirement C application code
# (linked library variation)
FIO_TLS_TEST_BEARSSL_EXT:="\\n\
\#define _GNU_SOURCE\\n\
\#include <stdlib.h>\\n\
\#include <bearssl.h>\\n\
int main(void) {\\n\
}\\n\
"

# OpenSSL requirement C application code
FIO_TLS_TEST_OPENSSL:="\\n\
\#define _GNU_SOURCE\\n\
\#include <stdlib.h>\\n\
\#include <openssl/bio.h> \\n\
\#include <openssl/err.h> \\n\
\#include <openssl/ssl.h> \\n\
\#if OPENSSL_VERSION_NUMBER < 0x10100000L \\n\
\#error \"OpenSSL version too small\" \\n\
\#endif \\n\
int main(void) { \\n\
  SSL_library_init(); \\n\
  SSL_CTX *ctx = SSL_CTX_new(TLS_method()); \\n\
  SSL *ssl = SSL_new(ctx); \\n\
  BIO *bio = BIO_new_socket(3, 0); \\n\
  BIO_up_ref(bio); \\n\
  SSL_set0_rbio(ssl, bio); \\n\
  SSL_set0_wbio(ssl, bio); \\n\
}\\n\
"

# automatic library adjustments for possible BearSSL library
LIB_PRIVATE_SUBFOLDERS:=$(LIB_PRIVATE_SUBFOLDERS) $(if $(wildcard lib/bearssl),bearssl)

# default OpenSSL flags
OPENSSL_CFLAGS:=
OPENSSL_LIBS:=
OPENSSL_LDFLAGS:="-lssl" "-lcrypto"
# detect OpenSSL flags using pkg-config, if available
ifeq ($(shell $(PKG_CONFIG) -- openssl >/dev/null 2>&1; echo $$?), 0)
	OPENSSL_CFLAGS:=$(shell $(PKG_CONFIG) --cflags openssl)
	OPENSSL_LDFLAGS:=$(shell $(PKG_CONFIG) --libs openssl)
endif


# add BearSSL/OpenSSL library flags (exclusive)
ifdef FIO_NO_TLS
else ifeq ($(call TRY_COMPILE, $(FIO_TLS_TEST_BEARSSL_SOURCE), $(EMPTY)), 0)
  $(info * Detected the BearSSL source code library, setting HAVE_BEARSSL)
  # TODO: when BearSSL support arrived, set the FIO_TLS_FOUND flag as well
	FLAGS+=HAVE_BEARSSL
else ifeq ($(call TRY_COMPILE, $(FIO_TLS_TEST_BEARSSL_EXT), "-lbearssl"), 0)
  $(info * Detected the BearSSL library, setting HAVE_BEARSSL)
  # TODO: when BearSSL support arrived, set the FIO_TLS_FOUND flag as well
	FLAGS+=HAVE_BEARSSL
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) bearssl
else ifeq ($(call TRY_COMPILE, $(FIO_TLS_TEST_OPENSSL), $(OPENSSL_CFLAGS) $(OPENSSL_LDFLAGS)), 0)
  $(info * Detected the OpenSSL library, setting HAVE_OPENSSL)
	FLAGS+=HAVE_OPENSSL FIO_TLS_FOUND
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) $(OPENSSL_LIBS)
	LDFLAGS+=$(OPENSSL_LDFLAGS)
	CFLAGS+=$(OPENSSL_CFLAGS)
	PKGC_REQ_OPENSSL=openssl >= 1.1, openssl < 1.2
	PKGC_REQ+=$$(PKGC_REQ_OPENSSL)
else
  $(info * No compatible SSL/TLS library detected.)
endif

# S2N TLS/SSL library: https://github.com/awslabs/s2n
ifeq ($(call TRY_COMPILE, "\#include <s2n.h>\\n int main(void) {}", "-ls2n") , 0)
  $(info * Detected the s2n library, setting HAVE_S2N)
	FLAGS:=$(FLAGS) HAVE_S2N
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) s2n
endif

endif # TEST4SSL
#############################################################################
# ZLib Library Detection
# (no need to edit)
#############################################################################
ifdef TEST4ZLIB

ifeq ($(call TRY_COMPILE, "\#include <zlib.h>\\nint main(void) {}", "-lz") , 0)
  $(info * Detected the zlib library, setting HAVE_ZLIB)
	FLAGS:=$(FLAGS) HAVE_ZLIB
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) z
	PKGC_REQ_ZLIB=zlib
	PKGC_REQ+=$$(PKGC_REQ_ZLIB)
endif

endif #TEST4ZLIB
#############################################################################
# PostgreSQL Library Detection
# (no need to edit)
#############################################################################
ifdef TEST4PG

ifeq ($(call TRY_COMPILE, "\#include <libpq-fe.h>\\n int main(void) {}", "-lpg") , 0)
  $(info * Detected the PostgreSQL library, setting HAVE_POSTGRESQL)
	FLAGS:=$(FLAGS) HAVE_POSTGRESQL
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) pg
else ifeq ($(call TRY_COMPILE, "\#include </usr/include/postgresql/libpq-fe.h>\\nint main(void) {}", "-lpg") , 0)
  $(info * Detected the PostgreSQL library, setting HAVE_POSTGRESQL)
	FLAGS:=$(FLAGS) HAVE_POSTGRESQL
	INCLUDE_STR:=$(INCLUDE_STR) -I/usr/include/postgresql
	LINKER_LIBS_EXT:=$(LINKER_LIBS_EXT) pg
endif

endif # TEST4PG
#############################################################################
# Endian  Detection
# (no need to edit)
#############################################################################
ifdef TEST4ENDIAN

ifeq ($(call TRY_COMPILE_AND_RUN, "int main(void) {int i = 1; return (int)(i & ((unsigned char *)&i)[sizeof(i)-1]);}\n",$(EMPTY)), 1)
  $(info * Detected Big Endian byte order.)
	FLAGS:=$(FLAGS) __BIG_ENDIAN__
else ifeq ($(call TRY_COMPILE_AND_RUN, "int main(void) {int i = 1; return (int)(i & ((unsigned char *)&i)[0]);}\n",$(EMPTY)), 1)
  $(info * Detected Little Endian byte order.)
	FLAGS:=$(FLAGS) __BIG_ENDIAN__=0
else
  $(info * Byte ordering (endianness) detection failed)
endif

endif # TEST4ENDIAN
#############################################################################
# Updated flags and final values
# (don't edit)
#############################################################################

FLAGS_STR=$(foreach flag,$(FLAGS),$(addprefix -D, $(flag)))
CFLAGS:=$(CFLAGS) -g $(CSTD) -fpic $(FLAGS_STR) $(WARNINGS) $(INCLUDE_STR) $(C_EXTRA_OPT)
CXXFLAGS:=$(CXXFLAGS) $(CXXSTD) -fpic  $(FLAGS_STR) $(WARNINGS) $(INCLUDE_STR) $(CXX_EXTRA_OPT)
LINKER_FLAGS=$(LDFLAGS) $(foreach lib,$(LINKER_LIBS),$(addprefix -l,$(lib))) $(foreach lib,$(LINKER_LIBS_EXT),$(addprefix -l,$(lib)))
CFLAGS_DEPENDENCY=-MT $@ -MMD -MP


# Build a "Requires:" string for the pkgconfig/facil.pc file
# unfortunately, leading or trailing commas are interpreted as
# "empty package name" by pkg-config, therefore we work around this by using
# $(strip ..).
# The following 2 lines are from the manual of GNU make
nullstring :=
space := $(nullstring) # end of line
comma := ,
$(eval PKGC_REQ_EVAL:=$(subst $(space),$(comma) ,$(strip $(PKGC_REQ))))

#############################################################################
# Tasks - Building
#############################################################################

$(NAME): build

build: | create_tree build_objects

build_objects: $(LIB_OBJS) $(MAIN_OBJS)
	@echo "* Linking..."
	@$(CCL) -o $(BIN) $^ $(OPTIMIZATION) $(LINKER_FLAGS)
	@echo "* Finished: $(BIN)"
	@$(DOCUMENTATION)

.PHONY : clean
clean: | _.___clean

.PHONY : %.___clean
%.___clean:
	-@rm -f $(BIN) 2> /dev/null || echo "" >> /dev/null
	-@rm -R -f $(TMP_ROOT) 2> /dev/null || echo "" >> /dev/null
	-@mkdir -p $(BUILDTREE) 2> /dev/null

.PHONY : run
run: | build
	@$(BIN)

.PHONY : db
db: | db.___clean
	DEBUG=1 $(MAKE) build
	$(DB) $(BIN)

.PHONY : create_tree
create_tree:
	-@mkdir -p $(BUILDTREE) 2> /dev/null

lib: | create_tree lib_build

$(DEST)/pkgconfig/facil.pc: makefile | libdump
	@mkdir -p $(DEST)/pkgconfig && \
	printf "\
Name: facil.io\\n\
Description: facil.io\\n\
Cflags: -I%s\\n\
Libs: -L%s -lfacil\\n\
Version: %s\\n\
Requires.private: %s\\n\
" $(realpath $(DEST)/../libdump/include) $(realpath $(DEST)) 0.7.x "$(PKGC_REQ_EVAL)" > $@

$(DEST)/libfacil.so: $(LIB_OBJS) | $(DEST)/pkgconfig/facil.pc
	@$(CCL) -shared -o $@ $^ $(OPTIMIZATION) $(LINKER_FLAGS)

lib_build: $(DEST)/libfacil.so
	@$(DOCUMENTATION)

%.o : %.c

#### no disassembler (normal / expected state)
ifndef DISAMS
$(TMP_ROOT)/%.o: %.c $(TMP_ROOT)/%.d
	@echo "* Compiling $<"
	@$(CC) -c $< -o $@ $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)

$(TMP_ROOT)/%.o: %.cpp $(TMP_ROOT)/%.d
	@echo "* Compiling $<"
	@$(CC) -c $< -o $@ $(CFLAGS_DEPENDENCY) $(CXXFLAGS) $(OPTIMIZATION)
	$(eval CCL=$(CXX))

$(TMP_ROOT)/%.o: %.c++ $(TMP_ROOT)/%.d
	@echo "* Compiling $<"
	@$(CC) -c $< -o $@ $(CFLAGS_DEPENDENCY) $(CXXFLAGS) $(OPTIMIZATION)
	$(eval CCL=$(CXX))

#### add diassembling stage (testing / slower)
else
$(TMP_ROOT)/%.o: %.c $(TMP_ROOT)/%.d
	@echo "* Compiling $<"
	@$(CC) -c $< -o $@ $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@$(DISAMS) $@ > $@.s

$(TMP_ROOT)/%.o: %.cpp $(TMP_ROOT)/%.d
	@echo "* Compiling $<"
	@$(CXX) -o $@ -c $< $(CFLAGS_DEPENDENCY) $(CXXFLAGS) $(OPTIMIZATION)
	$(eval CCL=$(CXX))
	@$(DISAMS) $@ > $@.s

$(TMP_ROOT)/%.o: %.c++ $(TMP_ROOT)/%.d
	@echo "* Compiling $<"
	@$(CXX) -o $@ -c $< $(CFLAGS_DEPENDENCY) $(CXXFLAGS) $(OPTIMIZATION)
	$(eval CCL=$(CXX))
	@$(DISAMS) $@ > $@.s
endif

$(TMP_ROOT)/%.d: ;

-include $(OBJS_DEPENDENCY)

#############################################################################
# Tasks - Testing
#############################################################################


ifneq ($(TEST_SRC),)

.PHONY : test/set_test_flag
test/set_test_flag:
	$(eval CFLAGS+=-DTEST=1)
	$(eval CXXFLAGS+=-DTEST=1)

.PHONY : test/set_debug_flags
test/set_debug_flags:
	$(eval OPTIMIZATION=-O0 -march=native -fsanitize=address -fno-omit-frame-pointer)
	$(eval CFLAGS+=-coverage -DDEBUG=1 -Werror)
	$(eval CXXFLAGS+=-coverage -DDEBUG=1)
	$(eval LINKER_FLAGS=-coverage -DDEBUG=1 $(LINKER_FLAGS))

.PHONY : test/build
test/build: | test/__.test_build
	@$(CCL) -o $(BIN) $(LIB_OBJS) $(TEST_OBJS) $(OPTIMIZATION) $(LINKER_FLAGS)

.PHONY : %.test_build
%.test_build: | test/set_test_flag $(LIB_OBJS) $(TEST_OBJS)
	@$(CCL) -o $(BIN) $(LIB_OBJS) $(TEST_OBJS) $(OPTIMIZATION) $(LINKER_FLAGS)

.PHONY : test/%.run
test/%.run:
	@$(BIN)

.PHONY : test
test: | cmake test/set_debug_flags test.___clean test.test_build test/test.run test_finished.___clean

.PHONY : test/optimized
test/optimized: | optimized.___clean cmake optimized.___clean optimized.test_build test/optimized.run

.PHONY : test/c99
test/c99:| c99.___clean
	@echo "* Starting C99 test"
	@CSTD=-std=c99 DEBUG=1 $(MAKE) test/build
	@echo "* C99 compilation success!"

.PHONY : test/ci
test/ci:| ci.___clean cmake test/set_debug_flags test/ci.___clean ci.test_build test/ci.run
	@echo "* testing complete."

.PHONY : test/stl
test/stl: | stl.___clean
	@echo "* Compiling facil.io STL test"
	@time $(CC) -c ./tests/stl_test.c -o $(TMP_ROOT)/stl_test.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@echo "* Linking STL test"
	@$(CC) -o $(BIN) $(TMP_ROOT)/stl_test.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@$(BIN)

.PHONY : test/core
test/core: | core.___clean
	@echo "* Compiling facil.io Core IO Library test"
	@time $(CC) -c ./tests/core_test.c -o $(TMP_ROOT)/core_test.o $(CFLAGS_DEPENDENCY) -DFIO_WEAK_TLS $(CFLAGS) $(OPTIMIZATION)
	@echo "* Linking Core IO Library test"
	@$(CC) -o $(BIN) $(TMP_ROOT)/core_test.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@$(BIN)

.PHONY : test/poll
test/poll:| poll.___clean
	@echo "* Starting FIO_FORCE_POLL test (testing poll engine)."
	@DEBUG=1 FIO_POLL=1 $(MAKE) test/core
	@echo "* FIO_FORCE_POLL testing complete."

.PHONY : test/cpp
test/cpp: | cpp.___clean
	@echo "* Compiling C++ test"
	@$(CXX) -c ./tests/cpp_test.cpp -o $(TMP_ROOT)/cpp_test.o $(CFLAGS_DEPENDENCY) $(CXXFLAGS) $(OPTIMIZATION)
	@echo "* Linking C++ test"
	@$(CXX) -o $(BIN) $(TMP_ROOT)/cpp_test.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@echo "* C++ compilation success!"
	@echo "* To run test:"
	@echo "  $(BIN)"

.PHONY : test/all
test/all: | test/optimized test/cpp test/c99 test/poll
	@$(MAKE) test/ci


.PHONY : test/collisions
test/collisions: | create_tree
	@echo "* Compiling tests/collisions.c"
	@$(CC) -c ./tests/collisions.c -o $(TMP_ROOT)/collisions.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION) -DFIO_WEAK_TLS
	@echo "* Linking"
	@$(CCL) -o $(BIN) $(TMP_ROOT)/collisions.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@echo "* Starting test:"
	@$(BIN)

.PHONY : test/malloc
test/malloc: | create_tree
	@$(CC) -c ./tests/malloc_speed.c -o $(TMP_ROOT)/malloc_speed.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@$(CCL) -o $(BIN) $(TMP_ROOT)/malloc_speed.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@$(BIN)

.PHONY : test/http1_parser
test/http1_parser: | create_tree
	@$(CC) -c ./tests/http1_parser.c -o $(TMP_ROOT)/http1_parser.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@$(CCL) -o $(BIN) $(TMP_ROOT)/http1_parser.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@$(BIN)

.PHONY : test/hpack
test/hpack: | create_tree
	@$(CC) -c ./tests/hpack.c -o $(TMP_ROOT)/hpack.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@$(CCL) -o $(BIN) $(TMP_ROOT)/hpack.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@$(BIN)

.PHONY : test/json
test/json: | create_tree
	@$(CC) -c ./tests/json_roundtrip.c -o $(TMP_ROOT)/json.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@$(CCL) -o $(BIN) $(TMP_ROOT)/json.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@$(BIN)

.PHONY : test/memchr
test/memchr: | create_tree
	@$(CC) -c ./tests/memchr_speed.c -o $(TMP_ROOT)/memchr.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@$(CCL) -o $(BIN) $(TMP_ROOT)/memchr.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@$(BIN)

.PHONY : test/random
test/random: | create_tree
	@$(CC) -c ./tests/random.c -o $(TMP_ROOT)/random.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@$(CCL) -o $(BIN) $(TMP_ROOT)/random.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@$(BIN)

.PHONY : test/url
test/url: | create_tree
	@$(CC) -c ./tests/parse_url.c -o $(TMP_ROOT)/url.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@$(CCL) -o $(BIN) $(TMP_ROOT)/url.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@$(BIN)

.PHONY : test/slowloris
test/slowloris: | create_tree
	@$(CC) -c ./tests/slowloris.c -o $(TMP_ROOT)/slowloris.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION)
	@$(CCL) -o $(BIN) $(TMP_ROOT)/slowloris.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@echo test a server for slowloris using: $(BIN)

endif

#############################################################################
# Tasks - Installers
#############################################################################

.PHONY : install/bearssl
install/bearssl: | remove/bearssl add/bearssl ;

.PHONY : add/bearssl
add/bearssl: | remove/bearssl
	-@echo " "
	-@echo "* Cloning BearSSL and copying source files to lib/bearssl."
	-@echo "  Please review the BearSSL license."
	@git clone https://www.bearssl.org/git/BearSSL tmp/bearssl
	@mkdir lib/bearssl
	-@find tmp/bearssl/src -name "*.*" -exec mv "{}" lib/bearssl \;
	-@find tmp/bearssl/inc -name "*.*" -exec mv "{}" lib/bearssl \;
	-@make clean

.PHONY : remove/bearssl
remove/bearssl:
	-@echo "* Removing existing BearSSL source files."
	-@rm -R -f lib/bearssl 2> /dev/null || echo "" >> /dev/null
	-@make clean


#############################################################################
# Tasks - library code dumping & CMake
#############################################################################

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

#############################################################################
# Tasks - make variable printout (test)
#############################################################################

# Prints the make variables, used for debugging the makefile
.PHONY : vars
vars:
	@echo "CC: $(CC)"
	@echo "CXX: $(CXX)"
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
	@echo "TEST_OBJS: $(TEST_OBJS)"
	@echo ""
	@echo "OBJS_DEPENDENCY: $(OBJS_DEPENDENCY)"
	@echo ""
	@echo "CFLAGS: $(CFLAGS)"
	@echo ""
	@echo "OPTIMIZATION: $(OPTIMIZATION)"
	@echo ""
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo ""
	@echo "LINKER_LIBS: $(LINKER_LIBS)"
	@echo ""
	@echo "LINKER_LIBS_EXT: $(LINKER_LIBS_EXT)"
	@echo ""
	@echo "LINKER_FLAGS: $(LINKER_FLAGS)"


