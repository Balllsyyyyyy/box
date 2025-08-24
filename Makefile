CC="gcc"
SRC="main.c"
BIN="box"
BITS="64"
MARCH="native"
MTUNE="native"
OPT="z"
GDB="0"
FLAGS="-lraylib -lm"

all:
	$(CC) $(SRC) -o $(BIN) -m$(BITS) -march=$(MARCH) -mtune=$(MTUNE) -O$(OPT) -g$(GDB) $(FLAGS)

clean:
	rm -f $(BIN)
