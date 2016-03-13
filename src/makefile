NAME=tryme
CC=@gcc
DB=@lldb
LIBS= -pthread -lssl -lcrypto
INCLUDE=/usr/local/include http ./
CFLAGS=-Wall -g -O3 $(addprefix -I, $(INCLUDE))
BINDIR=./../tmp
BUILDDIR=./../tmp
# SRC = $(shell find . -name *.c)
SRC = .
BIN=$(BINDIR)/$(NAME)
OBJS=$(addsuffix .o, $(addprefix $(BUILDDIR)/, $(notdir $(basename $(wildcard $(SRC)/*.c))))) $(addsuffix .o, $(addprefix $(BUILDDIR)/http/, $(notdir $(basename $(wildcard $(SRC)/http/*.c)))))
#-lm

$(NAME): build

build: $(OBJS)
	$(CC) -o $(BIN) $^ $(CFLAGS) $(LIBS)

$(BUILDDIR)/%.o: $(SRC)/%.c # $(SRC)/%.h
	$(CC) -o $@ -c $^ $(CFLAGS)

clean:
	-@rm -R $(BUILDDIR)
	-@mkdir $(BUILDDIR)
	-@mkdir $(BUILDDIR)/http

execute:
	@$(BIN)

run: clean build execute

db: clean build
	$(DB) $(BIN)
