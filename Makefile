CFLAGS = -g
CC = gcc

LIBS = -lpthread

PROM = basic.out #main.out

all: $(PROM)

# main.out: generic.h thread-pool.h thread-pool.c connection.c http.c main.c 
# 	$(CC) $(CFLAGS) -o main.out thread-pool.c connection.c http.c main.c $(LIBS)

basic.out: generic.h thread-pool.h thread-pool.c connection.c http.c tcp_server.c basic.c
	$(CC) $(CFLAGS) -o basic.out thread-pool.c connection.c http.c tcp_server.c basic.c $(LIBS)

clear:
	rm $(PROM)