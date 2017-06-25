### Output
# binary name and location
NAME=demo
OUT_ROOT=tmp

### Library folders
# the .c and .cpp source files root folder - subfolders are automatically included
LIB_ROOT=./lib
# publicly used subfolders in the lib root
LIB_PUBLIC_SUBFOLDERS=core core/types services http redis
# privately used subfolders in the lib root (this distinction is for CMake)
LIB_PRIVATE_SUBFOLDERS=bscrypt bscrypt/bscrypt

### Development folders
# The development, non-library .c file(s) (i.e., the one with `int main(void)`.
MAIN_ROOT=./dev
# Development subfolders in the main root
MAIN_SUBFOLDERS=

### Build Root
# temporary folder will be cleared out and deleted between fresh builds
# All object files will be placed in this folder
TMP_ROOT=tmp

### Compiler & Linker flags
# any librries required (write in full flags)
LINKER_FLAGS=-lpthread
# optimization level.
OPTIMIZATION=-O3 -march=native
# Warnings... i.e. -Wpedantic -Weverything -Wno-format-pedantic
WARNINGS= -Wall -Wextra -Wno-missing-field-initializers
# any extra include folders, space seperated list
INCLUDE= ./
# any preprocessosr defined flags we want, space seperated list (i.e. DEBUG )
FLAGS=DEBUG

### Helpers
# The library details for CMake incorporation. Can be safely removed.
CMAKE_LIBFILE_NAME=CMakeLists.txt
# dumps all library files in one place
DUMP_LIB=libdump


##############
## OS specific data - compiler, assembler etc.

ifneq ($(OS),Windows_NT)
	OS := $(shell uname)
endif
ifeq ($(OS),Darwin) # Run MacOS commands
	# c compiler
	CC=@gcc
	# c++ compiler
	CPP=@g++
	# debugger
	DB=@lldb
	# disassemble tool. Use stub to disable.
	DISAMS=@otool -tVX
	# documentation commands
	# DOCUMENTATION=cldoc generate $(INCLUDE_STR) -- --output ./html $(foreach dir, $(SRCDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.h*))


else
	# c compiler
	CC=@gcc
	# c++ compiler
	CPP=@g++
	# debugger
	DB=@gdb
	# disassemble tool, leave undefined.
	# DISAMS=@otool -tVX
	DOCUMENTATION=

endif

#####################
# Auto computed values
BIN = $(OUT_ROOT)/$(NAME)

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
FLAGS_STR = $(foreach flag,$(FLAGS),$(addprefix -D, $(flag)))

OBJS = $(foreach source, $(SOURCES), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))

# the computed C flags
CFLAGS= -g -std=c11  $(FLAGS_STR) $(WARNINGS) $(OPTIMIZATION) $(INCLUDE_STR)
CPPFLAGS= -std=c++11 $(FLAGS_STR) $(WARNINGS) $(OPTIMIZATION) $(INCLUDE_STR)

########
## Main Tasks

$(NAME): cmake libdump build

build: $(OBJS)
	$(CCL) -o $(BIN) $^ $(OPTIMIZATION) $(LINKER_FLAGS)
	$(DOCUMENTATION)

ifdef DISAMS
$(TMP_ROOT)/%.o: %.c
	$(CC) -o $@ -c $^ $(CFLAGS)
	$(DISAMS) $@ > $@.s

$(TMP_ROOT)/%.o: %.cpp
	$(CPP) -o $@ -c $^ $(CPPFLAGS)
	$(eval CCL = $(CPP))
	$(DISAMS) $@ > $@.s

else
$(TMP_ROOT)/%.o: %.c
	$(CC) -o $@ -c $^ $(CFLAGS)

$(TMP_ROOT)/%.o: %.cpp
	$(CPP) -o $@ -c $^ $(CPPFLAGS)
	$(eval CCL = $(CPP))
endif

.PHONY : clean
clean: cmake libdump
	-@rm $(BIN)
	-@rm -R $(TMP_ROOT)
	-@mkdir -p $(BUILDTREE)

.PHONY : execute
execute:
	@$(BIN)

.PHONY : run
run: | cmake libdump build execute

.PHONY : db
db: | clean build
	$(DB) $(BIN)


########
## Helper Tasks

ifndef DUMP_LIB
.PHONY : libdump
libdump:

else

.PHONY : libdump
libdump:
	-@rm -R $(DUMP_LIB)
	@mkdir $(DUMP_LIB)
	@mkdir $(DUMP_LIB)/src
	@mkdir $(DUMP_LIB)/include
	@mkdir $(DUMP_LIB)/all # except README.md files
	@cp -n $(foreach dir,$(LIBDIR_PUB), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^m]*)) $(DUMP_LIB)/all
	@cp -n $(foreach dir,$(LIBDIR_PRIV), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^m]*)) $(DUMP_LIB)/all
	@cp -n $(foreach dir,$(LIBDIR_PUB), $(wildcard $(addsuffix /, $(basename $(dir)))*.h*)) $(DUMP_LIB)/include
	@cp -n $(foreach dir,$(LIBDIR_PRIV), $(wildcard $(addsuffix /, $(basename $(dir)))*.h*)) $(DUMP_LIB)/include
	@cp -n $(foreach dir,$(LIBDIR_PUB), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^hm]*)) $(DUMP_LIB)/src
	@cp -n $(foreach dir,$(LIBDIR_PRIV), $(wildcard $(addsuffix /, $(basename $(dir)))*.[^hm]*)) $(DUMP_LIB)/src

endif

ifndef CMAKE_LIBFILE_NAME
.PHONY : cmake
cmake:

else

.PHONY : cmake
cmake:
	-@rm $(CMAKE_LIBFILE_NAME)
	@touch $(CMAKE_LIBFILE_NAME)
	@echo 'project(facil.io C)' >> $(CMAKE_LIBFILE_NAME)
	@echo '' >> $(CMAKE_LIBFILE_NAME)
	@echo 'find_package(Threads REQUIRED)' >> $(CMAKE_LIBFILE_NAME)
	@echo '' >> $(CMAKE_LIBFILE_NAME)
	@echo 'set(facil.io_SOURCES' >> $(CMAKE_LIBFILE_NAME)
	@$(foreach src,$(LIBSRC),echo '  $(src)' >> $(CMAKE_LIBFILE_NAME);)
	@echo ')' >> $(CMAKE_LIBFILE_NAME)
	@echo '' >> $(CMAKE_LIBFILE_NAME)
	@echo 'add_library(facil.io $${facil.io_SOURCES})' >> $(CMAKE_LIBFILE_NAME)
	@echo 'target_link_libraries(facil.io PRIVATE Threads::Threads)' >> $(CMAKE_LIBFILE_NAME)
	@echo 'target_include_directories(facil.io' >> $(CMAKE_LIBFILE_NAME)
	@$(foreach src,$(LIBDIR_PUB),echo '  PUBLIC  $(src)' >> $(CMAKE_LIBFILE_NAME);)
	@$(foreach src,$(LIBDIR_PRIV),echo '  PRIVATE $(src)' >> $(CMAKE_LIBFILE_NAME);)
	@echo ')' >> $(CMAKE_LIBFILE_NAME)
	@echo '' >> $(CMAKE_LIBFILE_NAME)

endif

# Prints the make variables, used for debugging the makefile
.PHONY : vars
vars:
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
	@echo "SOURCES: $(SOURCES)"
	@echo ""
	@echo "OBJS: $(OBJS)"
	@echo ""
	@echo "CFLAGS: $(CFLAGS)"
	@echo ""
	@echo "CPPFLAGS: $(CPPFLAGS)"
