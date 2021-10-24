CFLAGS = -g
CC = gcc

LIBS = -lpthread

PROM = basic.out chat-client.out chat-server.out

all: $(PROM)

# main.out: generic.h thread-pool.h thread-pool.c connection.c http.c main.c 
# 	$(CC) $(CFLAGS) -o main.out thread-pool.c connection.c http.c main.c $(LIBS)

# basic.out: generic.h thread-pool.h thread-pool.c connection.c http.c tcp_server.c basic.c
# 	$(CC) $(CFLAGS) -o basic.out thread-pool.c connection.c http.c tcp_server.c basic.c $(LIBS)

basic.out: thread-pool.o tcp-server.o http.o connection.o basic.c
	$(CC) $(CFLAGS) -o basic.out thread-pool.o tcp-server.o http.o connection.o basic.c $(LIBS)

chat-client.out: generic.h chat.h readwrite.c chat-client.c
	$(CC) $(CFLAGS) -o chat-client.out readwrite.c chat-client.c $(LIBS)

chat-server.out: chat.h thread-pool.o tcp-server.o http.o connection.o chat-server.c
	$(CC) $(CFLAGS) -o chat-server.out thread-pool.o tcp-server.o http.o connection.o chat-server.c $(LIBS)

http.o: generic.h
connection.o: generic.h
tcp-server.o: generic.h 
thread-pool.o: thread-pool.h

clean:
	rm $(PROM) *.o