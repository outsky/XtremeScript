CFLAGS = -g -Wall -std=c99 -D_GNU_SOURCE

LIBS = -lm

ALL_O = main.o lib.o list.o xasm.o xvm.o lexer.o

xs: $(ALL_O)
	cc -o $@ $(CFLAGS) $(ALL_O) $(LIBS)

main.o: main.c
lib.o: lib.h lib.c
list.o: list.h list.c
xasm.o: xasm.h xasm.c
xvm.o: xvm.h xvm.c
lexer.o: lexer.h lexer.c

clean:
	rm -f *.o xs.out
