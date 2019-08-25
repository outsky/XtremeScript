BIN = xs

CFLAGS = -g -Wall -std=c99 -D_GNU_SOURCE

LIBS = -lm -lc

ALL_O = main.o lib.o list.o xasm.o xvm.o lexer.o parser.o icode.o emitter.o

$(BIN): $(ALL_O)
	cc -o $@ $(CFLAGS) $(ALL_O) $(LIBS)

main.o: main.c
lib.o: lib.h lib.c
list.o: list.h list.c
xasm.o: xasm.h xasm.c
xvm.o: xvm.h xvm.c
lexer.o: lexer.h lexer.c
parser.o: parser.h parser.c
icode.o: icode.h icode.c
emitter.o: emitter.h emitter.c

clean:
	rm -f $(BIN) *.o 

test:
	./tester $(BIN)
