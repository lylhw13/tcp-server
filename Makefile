CFLAGS = -g
CC = gcc

LIBS = -lpthread

PROM = main.out

all: $(PROM)

main.out: generic.h thread-pool.h thread-pool.c http.c main.c 
	$(CC) $(CFLAGS) -o main.out thread-pool.c http.c main.c $(LIBS)

clear:
	rm $(PROM)