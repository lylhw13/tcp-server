INC_DIR = ../../
CFLAGS = -g -I$(INC_DIR)
CC = gcc

LIBS = -lpthread

# SRCFOLDER
SRCDIR = ../..
# VPATH = $(SRCDIR)
SOURCE = $(wildcard  $(SRCDIR)/*.h $(SRCDIR)/*.c)
# $(info var in $(SOURCE))

PROM = basic.out

all: $(PROM)

basic.out: $(SOURCE) basic.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# chat-client.out: generic.h chat.h http.o readwrite.c chat-client.c
# 	$(CC) $(CFLAGS) -o chat-client.out http.o readwrite.c chat-client.c $(LIBS)

# chat-server.out: chat.h thread-pool.o tcp-server.o http.o connection.o chat-server.c
# 	$(CC) $(CFLAGS) -o chat-server.out thread-pool.o tcp-server.o http.o connection.o chat-server.c $(LIBS)

# http.o: generic.h
# connection.o: generic.h
# tcp-server.o: generic.h 
# thread-pool.o: thread-pool.h

clean:
	rm $(PROM)