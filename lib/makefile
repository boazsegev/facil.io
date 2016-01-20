NAME=tryme
CC=@gcc
DB=@lldb
LIBS=
INCLUDE= /usr/local/include,./include,./lib
CFLAGS=-Wall -g -O3 -I$(INCLUDE)
BINDIR=./../tmp
BUILDDIR=./../tmp
# SRC = $(shell find . -name *.c)
SRC = .
BIN=$(BINDIR)/$(NAME)
OBJS=$(addsuffix .o, $(addprefix $(BUILDDIR)/, $(notdir $(basename $(wildcard $(SRC)/*.c)))))
#-lm

$(NAME): build

build: $(OBJS)
	$(CC) -o $(BIN) $^ $(CFLAGS) $(LIBS)

$(BUILDDIR)/%.o: $(SRC)/%.c # $(SRC)/%.h
	$(CC) -o $@ -c $^ $(CFLAGS) $(LIBS)

clean_build:
	-@rm $(BUILDDIR)/*.o

clean_bin:
	-@rm $(BIN)

clean: clean_build clean_bin

execute:
	@$(BIN)

run: clean build execute

db: clean build
	$(DB) $(BIN)
