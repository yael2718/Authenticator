CC = gcc -Wall -g -O3

.PHONY: clean

main: main.o uECC.o
	$(CC) -o main main.o uECC.o

main.o: main.c uECC.h uECC_vli.h
	$(CC) -c main.c

uECC.o: uECC.c uECC.h uECC_vli.h
	$(CC) -c uECC.c

clean:
	rm -f main *.o