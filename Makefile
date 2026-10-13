CC      := gcc
CFLAGS  := -Wall -Wextra -std=gnu11 -Iinclude -Isrc -pthread
LDFLAGS := -pthread -lrt
SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:.c=.o)
BIN     := crm

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	./$(BIN) 12

clean:
	rm -f $(OBJ) $(BIN)
