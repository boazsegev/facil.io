# binary name and location
NAME=demo
OUT_ROOT=./tmp
# temporary folder will be cleared out and deleted between fresh builds
# All object files will be placed in this folder
TMP_ROOT=./tmp
# the .c and .cpp source files root folder - subfolders are automatically included
SRC_ROOT=.
# any allowed subfolders in the src root 
SRC_SUB_FOLDERS=src src/http

LIBS=-pthread -lssl -lcrypto
INCLUDE=/usr/local/include

CC=@gcc
CPP=@g++
DB=@lldb
OPTIMIZATION=O3

#auto computed values
BIN = $(OUT_ROOT)/$(NAME)
SRCDIR = $(SRC_ROOT) $(foreach dir, $(SRC_SUB_FOLDERS), $(addsuffix /,$(basename $(SRC_ROOT)))$(dir))
SRC = $(foreach dir, $(SRCDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.c*))
BUILDTREE =$(foreach dir, $(SRCDIR), $(addsuffix /, $(basename $(TMP_ROOT)))$(basename $(dir)))
OBJS = $(foreach source, $(SRC), $(addprefix $(TMP_ROOT)/, $(addsuffix .o, $(basename $(source)))))
CCL = $(CC)

# the C flags
CFLAGS=-Wall -g -$(OPTIMIZATION) -std=c11 $(foreach dir,$(INCLUDE),$(addprefix -I, $(dir))) $(foreach dir,$(SRCDIR),$(addprefix -I, $(dir)))
CPPFLAGS= -Wall -$(OPTIMIZATION) -std=c++11 $(foreach dir,$(INCLUDE),$(addprefix -I, $(dir))) $(foreach dir,$(SRCDIR),$(addprefix -I, $(dir)))

$(NAME): build

build: $(OBJS)
	$(CCL) -o $(BIN) $^ -$(OPTIMIZATION) $(LIBS)

$(TMP_ROOT)/%.o: %.c
	$(CC) -o $@ -c $^ $(CFLAGS)

$(TMP_ROOT)/%.o: %.cpp
	$(CPP) -o $@ -c $^ $(CPPFLAGS)
	$(eval CCL = $(CPP))

clean:
	-@rm $(BIN)
	-@rm -R $(TMP_ROOT)
	-@mkdir -p $(BUILDTREE)

execute:
	@$(BIN)

run: | clean build execute

db: | clean build
	$(DB) $(BIN)

vars:
	@echo "BIN: $(BIN)"
	@echo ""
	@echo "SRCDIR: $(SRCDIR)"
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
