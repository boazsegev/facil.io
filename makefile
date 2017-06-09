# binary name and location
NAME=demo
OUT_ROOT=./tmp
# temporary folder will be cleared out and deleted between fresh builds
# All object files will be placed in this folder
TMP_ROOT=./tmp
# The none library .c file(s) (i.e., the one with `int main(void)`.
SRC_MAIN=main.c
# the .c and .cpp source files root folder - subfolders are automatically included
SRC_ROOT=
# any allowed subfolders in the src root
SRC_SUB_PUBLIC_FOLDERS=src/core src/core/types src/services src/http
# privately used subfolders in the src root (this distinction is for CMake)
SRC_SUB_PRIVATE_FOLDERS=src/bscrypt src/bscrypt/bscrypt src/http/unused
# any librries required (write in full flags)
LINKER_FLAGS=-lpthread
# any include folders, space seperated list
INCLUDE=
# optimization level.
OPTIMIZATION= -O3 -march=native -DDEBUG
# Warnings... i.e. -Wpedantic -Weverything -Wno-format-pedantic
WARNINGS= -Wall -Wextra -Wno-missing-field-initializers
# The library details for CMake incorporation. Can be safely removed.
CMAKE_LIBFILE_NAME=CMakeLists.txt

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

#auto computed values
BIN = $(OUT_ROOT)/$(NAME)
PUBSRCDIR = $(SRC_ROOT) $(foreach dir, $(SRC_SUB_PUBLIC_FOLDERS), $(addsuffix /,$(basename $(SRC_ROOT)))$(dir))
PRIVSRCDIR = $(foreach dir, $(SRC_SUB_PRIVATE_FOLDERS), $(addsuffix /,$(basename $(SRC_ROOT)))$(dir))
SRCDIR = $(PUBSRCDIR) $(PRIVSRCDIR)
LIBSRC = $(foreach dir, $(SRCDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.c*))
BUILDTREE =$(foreach dir, $(SRCDIR), $(addsuffix /, $(basename $(TMP_ROOT)))$(basename $(dir)))
# LIB_OBJS = $(foreach source, $(SRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))
CCL = $(CC)
INCLUDE_STR = $(foreach dir,$(INCLUDE),$(addprefix -I, $(dir))) $(foreach dir,$(SRCDIR),$(addprefix -I, $(dir)))
SRC = $(SRC_MAIN) $(LIBSRC)
OBJS = $(foreach source, $(SRC_MAIN) $(LIBSRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))

# the C flags
CFLAGS= -g -std=c11 $(WARNINGS) $(OPTIMIZATION) $(INCLUDE_STR)
CPPFLAGS= -std=c++11 $(WARNINGS) $(OPTIMIZATION) $(INCLUDE_STR)

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
	@$(foreach src,$(PUBSRCDIR),echo '  PUBLIC  $(src)' >> $(CMAKE_LIBFILE_NAME);)
	@$(foreach src,$(PRIVSRCDIR),echo '  PRIVATE $(src)' >> $(CMAKE_LIBFILE_NAME);)
	@echo ')' >> $(CMAKE_LIBFILE_NAME)
	@echo '' >> $(CMAKE_LIBFILE_NAME)

endif

.PHONY : vars
vars:
	@echo "BIN: $(BIN)"
	@echo ""
	@echo "PUBSRCDIR: $(PUBSRCDIR)"
	@echo ""
	@echo "PRIVSRCDIR: $(PRIVSRCDIR)"
	@echo ""
	@echo "SRCDIR: $(SRCDIR)"
	@echo ""
	@echo "LIBSRC: $(LIBSRC)"
	@echo ""
	@echo "SRC_MAIN: $(SRC_MAIN)"
	@echo ""
	@echo "SRC: $(SRC)"
	@echo ""
	@echo "BUILDTREE: $(BUILDTREE)"
	@echo ""
	@echo "OBJS: $(OBJS)"
	@echo ""
	@echo "CFLAGS: $(CFLAGS)"
	@echo ""
	@echo "CPPFLAGS: $(CPPFLAGS)"
