CFLAGS = -g -Wall -std=c99 -D_GNU_SOURCE

xs: list.o main.o xasm.o lib.o xvm.o
	cc -o xs.out $(CFLAGS) list.o main.o xasm.o lib.o xvm.o -lm

xvm.o: xvm.h xvm.c
lib.o: lib.h lib.c
xasm.o: xasm.h xasm.c
list.o: list.h list.c
main.o: main.c

clean:
	rm -f *.o xs.out
