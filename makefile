xs: list.o main.o xasm.o lib.o xvm.o
	cc -g -o xs.out list.o main.o xasm.o lib.o xvm.o -lm

xvm.o: xvm.h xvm.c
	cc -g -c xvm.c

lib.o: lib.h lib.c
	cc -g -c lib.c

xasm.o: xasm.h xasm.c
	cc -g -c xasm.c

list.o: list.h list.c
	cc -g -c list.c

main.o: main.c
	cc -g -c main.c

clean:
	rm -f *.o xs.out
