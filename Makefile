CC = gcc
SRC = main.c
BIN = box

BITS = 64
MARCH = native
MTUNE = native
OPT = z
GDB = 0

CFLAGS = -m$(BITS) -march=$(MARCH) -mtune=$(MTUNE) -O$(OPT) -g$(GDB)
LDFLAGS = -lraylib -lm

all:
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)
