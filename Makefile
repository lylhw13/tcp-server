CFLAGS = -g
CC = gcc

LIBS = -lpthread

PROM = basic.out# chat_client.out chat_server.out

all: $(PROM)

# main.out: generic.h thread-pool.h thread-pool.c connection.c http.c main.c 
# 	$(CC) $(CFLAGS) -o main.out thread-pool.c connection.c http.c main.c $(LIBS)

# basic.out: generic.h thread-pool.h thread-pool.c connection.c http.c tcp_server.c basic.c
# 	$(CC) $(CFLAGS) -o basic.out thread-pool.c connection.c http.c tcp_server.c basic.c $(LIBS)

basic.out: generic.h thread-pool.o connection.c http.c tcp-server.c basic.c
	$(CC) $(CFLAGS) -o basic.out thread-pool.o connection.c http.c tcp-server.c basic.c $(LIBS)

# tcp_server

thread-pool.o: thread-pool.h thread-pool.c
	$(CC) $(CFLAGS) -c -o thread-pool.o thread-pool.c $(LIBS)
clear:
	rm $(PROM)