# binary name and location
NAME=demo
OUT_ROOT=./tmp
# temporary folder will be cleared out and deleted between fresh builds
# All object files will be placed in this folder
TMP_ROOT=./tmp
# The none library .c file(s) (i.e., the one with `int main(void)`.
MAIN_ROOT=./dev
# any allowed subfolders in the main root
MAIN_SUBFOLDERS=http
# the .c and .cpp source files root folder - subfolders are automatically included
LIB_ROOT=./lib
# publicly used subfolders in the lib root
LIB_PUBLIC_SUBFOLDERS=core core/types services http
# privately used subfolders in the lib root (this distinction is for CMake)
LIB_PRIVATE_SUBFOLDERS=bscrypt bscrypt/bscrypt
# any librries required (write in full flags)
LINKER_FLAGS=-lpthread
# optimization level.
OPTIMIZATION=-O3 -march=native
# Warnings... i.e. -Wpedantic -Weverything -Wno-format-pedantic
WARNINGS= -Wall -Wextra -Wno-missing-field-initializers

# The library details for CMake incorporation. Can be safely removed.
CMAKE_LIBFILE_NAME=CMakeLists.txt

# any include folders, space seperated list
INCLUDE= ./

# any preprocessosr defined flags we want, space seperated list
FLAGS=DEBUG

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

MAINDIR = $(MAIN_ROOT) $(foreach dir, $(MAIN_SUBFOLDERS), $(addsuffix /,$(basename $(MAIN_ROOT)))$(dir))
MAINSRC = $(foreach dir, $(MAINDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.c*))

FOLDERS = $(LIBDIR) $(MAINDIR)
SOURCES = $(LIBSRC) $(MAINSRC)

BUILDTREE =$(foreach dir, $(FOLDERS), $(addsuffix /, $(basename $(TMP_ROOT)))$(basename $(dir)))

CCL = $(CC)

INCLUDE_STR = $(foreach dir,$(INCLUDE),$(addprefix -I, $(dir))) $(foreach dir,$(FOLDERS),$(addprefix -I, $(dir)))
FLAGS_STR = $(foreach flag,$(FLAGS),$(addprefix -I, $(flag)))

OBJS = $(foreach source, $(SOURCES), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))

# the C flags
CFLAGS= -g -std=c11  $(FLAGS_STR) $(WARNINGS) $(OPTIMIZATION) $(INCLUDE_STR)
CPPFLAGS= -std=c++11 $(FLAGS_STR) $(WARNINGS) $(OPTIMIZATION) $(INCLUDE_STR)

$(NAME): build

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
clean: cmake
	-@rm $(BIN)
	-@rm -R $(TMP_ROOT)
	-@mkdir -p $(BUILDTREE)

.PHONY : execute
execute:
	@$(BIN)

.PHONY : run
run: | cmake build execute

.PHONY : db
db: | cmake clean build
	$(DB) $(BIN)

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
