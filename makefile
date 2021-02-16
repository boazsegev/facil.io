#############################################################################
# This makefile was composed for facil.io
#
# Copyright (c) 2016-2020 Boaz Segev
# License MIT or ISC
#
# This makefile SHOULD be easilty portable and Should work on any POSIX
# system for any project... under the following assumptions:
#
# * If your code has a `main` function, that code should be placed in the
#   `MAIN_ROOT` folder and/or `MAIN_SUBFOLDERS` (i.e., `./src`).
#
# * If your code provides a library style API, the published functions are
#   in the `LIB_ROOT` folder and/or `LIB_XXX_SUBFOLDERS` (i.e., `./lib`).
#
# * If you want to concat a number of header / source / markdown (docs) files
#   than place them in the `LIB_CONCAT_FOLDER`.
#
# * Test files are independent (each test files compiles and runs as is) and
#   placed in the `TEST_ROOT` folder (i.e., `./tests`).
#
#   Run tests (i.e., the test file `foo.c`) with:       `make test/foo`
#   Run tests after linking with the library code with: `make test/lib/foo`
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
DUMP_LIB?=libdump

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
LIB_PUBLIC_SUBFOLDERS=facil facil/tls facil/fiobj facil/cli facil/http facil/http/parsers facil/redis

# privately used subfolders in the lib root (this distinction is only relevant for CMake)
LIB_PRIVATE_SUBFOLDERS=

#############################################################################
# Single Library Concatenation
#############################################################################

# a folder containing code that should be unified into a single file
#
# Note: files will be unified in the same order the system provides (usually, file anme)
LIB_CONCAT_FOLDER?=
# the path and file name to use when unifying *.c, *.h, and *.md files (without extension).
LIB_CONCAT_TARGET?=

#############################################################################
# Test Source Code Folder
#############################################################################

# Testing folder
TEST_ROOT=tests
# The default test file to run when running: make test (without the  C extension)
TEST_DEFAULT=tests

#############################################################################
# CMake Support
#############################################################################

# The library details for CMake incorporation. Can be safely removed.
CMAKE_FILENAME=CMakeLists.txt
# Project name to be stated in the CMakeFile
CMAKE_PROJECT=facil.io
# Space delimited list of required packages
CMAKE_REQUIRE_PACKAGE=Threads

#############################################################################
# Compiler / Linker Settings
#############################################################################

# any libraries required (only names, ommit the "-l" at the begining)
LINKER_LIBS=pthread m
# optimization level.
OPTIMIZATION=-O2 -march=native -DNDEBUG
# optimization level in debug mode.
OPTIMIZATION_DEBUG=-O0 -march=native -fsanitize=address -fsanitize=thread -fsanitize=undefined -fno-omit-frame-pointer
# Warnings... i.e. -Wpedantic -Weverything -Wno-format-pedantic
WARNINGS=-Wshadow -Wall -Wextra -Wpedantic -Wno-missing-field-initializers -Wno-embedded-directive
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
CXX_EXTRA_OPT:=-Wno-keyword-macro -Wno-vla-extension -Wno-c99-extensions -Wno-zero-length-array -Wno-variadic-macros
# c standard   (if any, without the `-std=` prefix, i.e.: c11)
CSTD?=gnu11
# c++ standard (if any, without the `-std=` prefix, i.e.: c++11)
CXXSTD?=gnu++11
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
  OPTIMIZATION:=$(OPTIMIZATION_DEBUG)
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
TEST4SOCKET:=1    # --- tests for socket library linker flags
TEST4CRYPTO:=1    # HAVE_OPENSSL / HAVE_BEARSSL + HAVE_SODIUM
TEST4SENDFILE:=1  # HAVE_SENDFILE
TEST4TM_ZONE:=1   # HAVE_TM_TM_ZONE
TEST4ZLIB:=       # HAVE_ZLIB
TEST4PG:=         # HAVE_POSTGRESQL
TEST4ENDIAN:=1    # __BIG_ENDIAN__=?

#############################################################################
# facil.io compilation flag helpers
#############################################################################

# add FIO_PRINT_STATE flag if requested
ifdef FIO_PRINT
  $(warning FIO_PRINT_STATE is deprecated. FIO_PRINT support will be removed soon.)
  FLAGS:=$(FLAGS) FIO_PRINT_STATE=$(FIO_PRINT)
endif

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

FOLDERS=$(LIBDIR) $(MAINDIR) $(TEST_ROOT)
SOURCES=$(LIBSRC) $(MAINSRC)

BUILDTREE=$(TMP_ROOT) $(foreach dir, $(FOLDERS), $(addsuffix /, $(basename $(TMP_ROOT)))$(basename $(dir)))

CCL=$(CC)

INCLUDE_STR=$(foreach dir,$(INCLUDE),$(addprefix -I, $(dir))) $(foreach dir,$(FOLDERS),$(addprefix -I, $(dir)))

MAIN_OBJS=$(foreach source, $(MAINSRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))
LIB_OBJS=$(foreach source, $(LIBSRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))

OBJS_DEPENDENCY:=$(LIB_OBJS:.o=.d) $(MAIN_OBJS:.o=.d) 

#############################################################################
# Combining single-file library
#############################################################################

ifdef LIB_CONCAT_FOLDER
ifdef LIB_CONCAT_TARGET
LIB_CONCAT_HEADERS=$(wildcard $(LIB_CONCAT_FOLDER)/*.h)
LIB_CONCAT_SOURCES=$(wildcard $(LIB_CONCAT_FOLDER)/*.c)
LIB_CONCAT_DOCS=$(wildcard $(LIB_CONCAT_FOLDER)/*.md)
ifneq ($(LIB_CONCAT_HEADERS), $(EMPTY))
  $(info * Building single-file header: $(LIB_CONCAT_TARGET).h)
  $(shell rm $(LIB_CONCAT_TARGET).h 2> /dev/null)
  $(shell cat $(LIB_CONCAT_FOLDER)/*.h >> $(LIB_CONCAT_TARGET).h)
endif
ifneq ($(LIB_CONCAT_SOURCES), $(EMPTY))
  $(info * Building single-file source: $(LIB_CONCAT_TARGET).c)
  $(shell rm $(LIB_CONCAT_TARGET).c 2> /dev/null)
  $(shell cat $(LIB_CONCAT_FOLDER)/*.c >> $(LIB_CONCAT_TARGET).c)
endif
ifneq ($(LIB_CONCAT_DOCS), $(EMPTY))
  $(info * Building documentation: $(LIB_CONCAT_TARGET).md)
  $(shell rm $(LIB_CONCAT_TARGET).md 2> /dev/null)
  $(shell cat $(LIB_CONCAT_FOLDER)/*.md >> $(LIB_CONCAT_TARGET).md)
endif

endif
endif

#############################################################################
# TRY_RUN, TRY_COMPILE and TRY_COMPILE_AND_RUN functions
#
# Call using $(call TRY_COMPILE, code, compiler_flags)
#
# Returns shell code as string: "0" (success) or non-0 (failure)
#
# TRY_COMPILE_AND_RUN returns the program's shell code as string.
#############################################################################

TRY_RUN=$(shell $(1) >> /dev/null 2> /dev/null; echo $$?;)
TRY_COMPILE=$(shell printf $(1) | $(CC) $(INCLUDE_STR) $(CFLAGS) -xc -o /dev/null - $(LDFLAGS) $(2) >> /dev/null 2> /dev/null ; echo $$? 2> /dev/null)
TRY_COMPILE_AND_RUN=$(shell printf $(1) | $(CC) $(INCLUDE_STR) $(CFLAGS) -xc -o ./___fio_tmp_test_ - $(LDFLAGS) $(2) 2> /dev/null ; ./___fio_tmp_test_ >> /dev/null 2> /dev/null; echo $$?; rm ./___fio_tmp_test_ 2> /dev/null)
EMPTY:=

#############################################################################
# GCC bug handling.
#
# GCC might trigger a bug when -fipa-icf is enabled and (de)constructor
# functions are used (as in our case with -O2 or above).
#
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=70306
#############################################################################

ifeq ($(shell $(CC) -v 2>&1 | grep -o "^gcc version [0-7]\." | grep -o "^gcc version"),gcc version)
  OPTIMIZATION+=-fno-ipa-icf
  $(info * Disabled `-fipa-icf` optimization, might be buggy with this gcc version.)
endif

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
  $(warning No socket API detected - won't be able to compile facil.io)
endif

endif # TEST4SOCKET
#############################################################################
# SSL/ TLS Library Detection
# (no need to edit)
#############################################################################
ifdef TEST4CRYPTO

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
ifeq ($(shell $(PKG_CONFIG) -- libsodium >/dev/null 2>&1; echo $$?), 0)
  LIBSODIUM_CFLAGS:=$(shell $(PKG_CONFIG) --cflags libsodium)
  LIBSODIUM_LDFLAGS:=$(shell $(PKG_CONFIG) --libs libsodium)
else
  LIBSODIUM_CFLAGS:=
  LIBSODIUM_LDFLAGS:=-lsodium
endif


# add BearSSL/OpenSSL library flags (exclusive)
ifdef FIO_NO_TLS
  $(info * Skipping crypto library detection.)
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
else ifeq ($(call TRY_COMPILE, "\#include <sodium.h.h>\\n int main(void) {}", $(LIBSODIUM_CFLAGS) $(LIBSODIUM_LDFLAGS)) , 0)
  # Sodium Crypto Library: https://doc.libsodium.org/usage
  $(info * Detected the Sodium library, setting HAVE_SODIUM)
  FLAGS:=$(FLAGS) HAVE_SODIUM
  LDFLAGS+=$(LIBSODIUM_CFLAGS)
  CFLAGS+=$(LIBSODIUM_LDFLAGS)
else
  $(info * No compatible SSL/TLS library detected.)
endif # FIO_NO_TLS

endif # TEST4CRYPTO
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
CFLAGS:=$(CFLAGS) -g -std=$(CSTD) -fpic $(FLAGS_STR) $(WARNINGS) $(INCLUDE_STR) $(C_EXTRA_OPT)
CXXFLAGS:=$(CXXFLAGS) -std=$(CXXSTD) -fpic  $(FLAGS_STR) $(WARNINGS) $(INCLUDE_STR) $(CXX_EXTRA_OPT)
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
# Task Settings - Compile Time Measurement
#############################################################################
TIME_TEST_CMD:=which time
ifeq ($(call TRY_RUN, $(TIME_TEST_CMD), $(EMPTY)), 0)
  CC:=time $(CC)
  CXX:=time $(CXX)
endif

#############################################################################
# Tasks - Building
#############################################################################

$(NAME): build

build: | create_tree build_objects
	@echo "* Linking..."
	@$(CCL) -o $(BIN) $^ $(OPTIMIZATION) $(LINKER_FLAGS)
	@echo "* Finished: $(BIN)"
	@$(DOCUMENTATION)

build_objects: $(LIB_OBJS) $(MAIN_OBJS)

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

.PHONY : set_debug_flags___
set_debug_flags___:
	$(eval OPTIMIZATION=$($(OPTIMIZATION_DEBUG)))
	$(eval CFLAGS+=-coverage -DDEBUG=1 -Werror)
	$(eval CXXFLAGS+=-coverage -DDEBUG=1)
	$(eval LINKER_FLAGS=-coverage -DDEBUG=1 $(LINKER_FLAGS))
	@echo "* Set Debug flags."

.PHONY : db
db: | db.___clean set_debug_flags___ build
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
	$(eval LINKER_FLAGS+= -lc++)

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
	$(eval LINKER_FLAGS+= -lc++)
	@$(DISAMS) $@ > $@.s

$(TMP_ROOT)/%.o: %.c++ $(TMP_ROOT)/%.d
	@echo "* Compiling $<"
	@$(CXX) -o $@ -c $< $(CFLAGS_DEPENDENCY) $(CXXFLAGS) $(OPTIMIZATION)
	$(eval CCL=$(CXX))
	$(eval LINKER_FLAGS+= -lc++)
	@$(DISAMS) $@ > $@.s
endif

$(TMP_ROOT)/%.d: ;

-include $(OBJS_DEPENDENCY)

#############################################################################
# Tasks - Testing
#
# Tasks:
#
# - test/cpp (tests CPP header / compilation)
# - test/lib/db/XXX (test, build library, use DEBUG, compile and run XXX.c)
# - test/lib/XXX (test, build library, compile and run XXX.c)
# - test/db/XXX (test, use DEBUG, compile and run XXX.c)
# - test/XXX (test, compile and run XXX.c)
#
#############################################################################

.PHONY : test_set_test_flag___
test_set_test_flag___:
	$(eval CFLAGS+=-DTEST=1 -DFIO_WEAK_TLS)
	$(eval CXXFLAGS+=-DTEST=1 -DFIO_WEAK_TLS)
	@echo "* Set testing flags."

# test/cpp will try to compile a source file using C++ to test header integration
.PHONY : test/cpp
test/cpp: | create_tree test_set_test_flag___ 
	$(eval BIN:=$(DEST)/cpp)
	@echo "* Compiling $(TEST_ROOT)/cpp.cpp"
	@$(CXX) -c $(TEST_ROOT)/cpp.cpp -o $(TMP_ROOT)/cpp.o $(CFLAGS_DEPENDENCY) $(CXXFLAGS) $(OPTIMIZATION) 
	@echo "* Linking"
	@$(CCL) -o $(BIN) $(TMP_ROOT)/cpp.o $(LINKER_FLAGS) -lc++ $(OPTIMIZATION)
	@echo "* Compilation of C++ variation successful."

# test/cpp will try to compile a source file using C++ to test header integration
.PHONY : test/db/cpp
test/db/cpp: | create_tree set_debug_flags___ test_set_test_flag___ 
	$(eval BIN:=$(DEST)/cpp)
	@echo "* Compiling $(TEST_ROOT)/cpp.cpp"
	@$(CXX) -c $(TEST_ROOT)/cpp.cpp -o $(TMP_ROOT)/cpp.o $(CFLAGS_DEPENDENCY) $(CXXFLAGS) $(OPTIMIZATION) 
	@echo "* Linking"
	@$(CCL) -o $(BIN) $(TMP_ROOT)/cpp.o $(LINKER_FLAGS) -lc++ $(OPTIMIZATION)
	@echo "* Compilation of C++ variation successful."

# test/build/db/XXX will set DEBUG, compile the library and run tests/XXX.c
.PHONY : test/lib/db/%
test/lib/db/%: | create_tree set_debug_flags___ test_set_test_flag___ $(LIB_OBJS)
	$(eval BIN:=$(DEST)/$*)
	@echo "* Compiling $(TEST_ROOT)/$*.c"
	@$(CC) -c $(TEST_ROOT)/$*.c -o $(TMP_ROOT)/$*.o -DTEST_WITH_LIBRARY $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION) 
	@echo "* Linking"
	@$(CCL) -o $(BIN) $(TMP_ROOT)/$*.o $(LIB_OBJS) $(LINKER_FLAGS) $(OPTIMIZATION)
	@echo "* Starting test:"
	@$(BIN)


# test/build/XXX will compile the library and compile and run tests/XXX.c
.PHONY : test/lib/%
test/lib/%: | create_tree test_set_test_flag___  $(LIB_OBJS)
	$(eval BIN:=$(DEST)/$*)
	@echo "* Compiling $(TEST_ROOT)/$*.c"
	@$(CC) -c $(TEST_ROOT)/$*.c -o $(TMP_ROOT)/$*.o -DTEST_WITH_LIBRARY $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION) 
	@echo "* Linking"
	@$(CCL) -o $(BIN) $(TMP_ROOT)/$*.o $(LIB_OBJS) $(LINKER_FLAGS) $(OPTIMIZATION)
	@echo "* Starting test:"
	@$(BIN)

# test/build/XXX will set DEBUG and compile and run tests/XXX.c
.PHONY : test/db/%
test/db/%: | create_tree set_debug_flags___ test_set_test_flag___
	$(eval BIN:=$(DEST)/$*)
	@echo "* Compiling $(TEST_ROOT)/$*.c"
	@$(CC) -c $(TEST_ROOT)/$*.c -o $(TMP_ROOT)/$*.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION) 
	@echo "* Linking"
	@$(CCL) -o $(BIN) $(TMP_ROOT)/$*.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@echo "* Starting test:"
	@$(BIN)


# test/build/XXX will compile and run tests/XXX.c
.PHONY : test/%
test/%: | create_tree test_set_test_flag___ 
	$(eval BIN:=$(DEST)/$*)
	@echo "* Compiling $(TEST_ROOT)/$*.c"
	@$(CC) -c $(TEST_ROOT)/$*.c -o $(TMP_ROOT)/$*.o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION) 
	@echo "* Linking"
	@$(CCL) -o $(BIN) $(TMP_ROOT)/$*.o $(LINKER_FLAGS) $(OPTIMIZATION)
	@echo "* Starting test:"
	@$(BIN)

ifneq ($(TEST_DEFAULT),)
.PHONY : test%
test: | create_tree test_set_test_flag___ 
	$(eval BIN:=$(DEST)/$(TEST_DEFAULT))
	@echo "* Compiling $(TEST_ROOT)/$(TEST_DEFAULT).c"
	@$(CC) -c $(TEST_ROOT)/$(TEST_DEFAULT).c -o $(TMP_ROOT)/$(TEST_DEFAULT).o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION) 
	@echo "* Linking"
	@$(CCL) -o $(BIN) $(TMP_ROOT)/$(TEST_DEFAULT).o $(LINKER_FLAGS) $(OPTIMIZATION)
	@echo "* Starting test:"
	@$(BIN)

.PHONY : test/db%
test/db: | create_tree set_debug_flags___ test_set_test_flag___
	$(eval BIN:=$(DEST)/$(TEST_DEFAULT))
	@echo "* Compiling $(TEST_ROOT)/$(TEST_DEFAULT).c"
	@$(CC) -c $(TEST_ROOT)/$(TEST_DEFAULT).c -o $(TMP_ROOT)/$(TEST_DEFAULT).o $(CFLAGS_DEPENDENCY) $(CFLAGS) $(OPTIMIZATION) 
	@echo "* Linking"
	@$(CCL) -o $(BIN) $(TMP_ROOT)/$(TEST_DEFAULT).o $(LINKER_FLAGS) $(OPTIMIZATION)
	@echo "* Starting test:"
	@$(BIN)
endif

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

ifndef CMAKE_FILENAME
.PHONY : cmake
cmake:
	@echo 'Missing CMake variables'

else

.PHONY : cmake
cmake:
	-@rm $(CMAKE_FILENAME) 2> /dev/null
	@touch $(CMAKE_FILENAME)
	@echo 'project($(CMAKE_PROJECT))' >> $(CMAKE_FILENAME)  
	@echo '' >> $(CMAKE_FILENAME)
	@echo 'cmake_minimum_required(VERSION 2.4)' >> $(CMAKE_FILENAME)
	@echo '' >> $(CMAKE_FILENAME)
	@$(foreach pkg,$(CMAKE_REQUIRE_PACKAGE),echo 'find_package($(pkg) REQUIRED)' >> $(CMAKE_FILENAME);)
	@echo '' >> $(CMAKE_FILENAME)
	@echo 'set($(CMAKE_PROJECT)_SOURCES' >> $(CMAKE_FILENAME)
	@$(foreach src,$(LIBSRC),echo '  $(src)' >> $(CMAKE_FILENAME);)
	@echo ')' >> $(CMAKE_FILENAME)
	@echo '' >> $(CMAKE_FILENAME)
	@echo 'add_library($(CMAKE_PROJECT) $${$(CMAKE_PROJECT)_SOURCES})' >> $(CMAKE_FILENAME)
	@echo 'target_link_libraries($(CMAKE_PROJECT)' >> $(CMAKE_FILENAME)
	@$(foreach src,$(LINKER_LIBS),echo '  PUBLIC $(src)' >> $(CMAKE_FILENAME);)
	@echo '  )' >> $(CMAKE_FILENAME)
	@echo 'target_include_directories($(CMAKE_PROJECT)' >> $(CMAKE_FILENAME)
	@$(foreach src,$(LIBDIR_PUB),echo '  PUBLIC  $(src)' >> $(CMAKE_FILENAME);)
	@$(foreach src,$(LIBDIR_PRIV),echo '  PRIVATE $(src)' >> $(CMAKE_FILENAME);)
	@echo ')' >> $(CMAKE_FILENAME)
	@echo '' >> $(CMAKE_FILENAME)

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


