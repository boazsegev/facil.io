NAME=demo
CC=@gcc
CPP=@g++
DB=@lldb
LIBS=-pthread -lssl -lcrypto
INCLUDE=/usr/local/include
OUT_ROOT=./tmp
SRC_ROOT=.
SRC_SUBFOLDERS=src src/http

#auto computed values
BIN=$(OUT_ROOT)/$(NAME)
SRCDIR= $(SRC_ROOT) $(foreach dir, $(SRC_SUBFOLDERS), $(addsuffix /,$(basename $(SRC_ROOT)))$(dir))
SRC = $(foreach dir, $(SRCDIR), $(wildcard $(addsuffix /, $(basename $(dir)))*.c*))
BUILDTREE=$(foreach dir, $(SRCDIR), $(addsuffix /, $(basename $(OUT_ROOT)))$(basename $(dir)))
OBJS= $(foreach source, $(SRC), $(addprefix $(OUT_ROOT)/, $(addsuffix .o, $(basename $(source)))))
CCL = $(CC)

# the C flags
CFLAGS=-Wall -g -std=c11 -O3 $(foreach dir,$(INCLUDE),$(addprefix -I, $(dir))) $(foreach dir,$(SRCDIR),$(addprefix -I, $(dir)))
CPPFLAGS= -Wall -std=c++11 $(foreach dir,$(INCLUDE),$(addprefix -I, $(dir))) $(foreach dir,$(SRCDIR),$(addprefix -I, $(dir)))

$(NAME): build

build: $(OBJS)
	$(CCL) -o $(BIN) $^ -O3 $(LIBS)

$(OUT_ROOT)/%.o: %.c # $(SRC)/%.h
	$(CC) -o $@ -c $^ $(CFLAGS)

$(OUT_ROOT)/%.o: %.cpp # $(SRC)/%.h
	$(CPP) -o $@ -c $^ $(CPPFLAGS)
	$(eval CCL = $(CPP))

clean:
	-@rm -R $(OUT_ROOT)
	-@mkdir -p $(BUILDTREE)

execute:
	@$(BIN)

run: clean build execute

db: clean build
	$(DB) $(BIN)
