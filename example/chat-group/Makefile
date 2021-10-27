INC_DIR = ../../

CFLAGS = -g -Wall -I$(INC_DIR)
CC = gcc

LIBS = -lpthread

SRCDIR = $(INC_DIR)
SOURCE = $(wildcard  $(SRCDIR)*.c)

PROM = chat-client.out chat-server.out

all: $(PROM)

chat-client.out: $(SOURCE) readwrite.c chat-client.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

chat-server.out: $(SOURCE) chat-server.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm $(PROM)