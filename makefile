test: list.o main.o
	cc -g -o test list.o main.o

list.o: list.h list.c
	cc -g -c list.c

main.o: main.c
	cc -g -c main.c

clean:
	rm -f *.o
