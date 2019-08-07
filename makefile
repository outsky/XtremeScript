CFLAGS = -g -Wall -std=c99 -D_GNU_SOURCE

LIBS = -lm

ALL_O = main.o lib.o list.o xasm.o xvm.o

xs: $(ALL_O)
	cc -o $@ $(CFLAGS) $(ALL_O) $(LIBS)

xvm.o: xvm.h xvm.c
lib.o: lib.h lib.c
xasm.o: xasm.h xasm.c
list.o: list.h list.c
main.o: main.c

clean:
	rm -f *.o xs.out
