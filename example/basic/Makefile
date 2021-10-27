INC_DIR = ../../src/

CFLAGS = -g -Wall -I$(INC_DIR)
CC = gcc

LIBS = -lpthread

SRCDIR = $(INC_DIR)
SOURCE = $(wildcard  $(SRCDIR)*.c)

PROM = basic.out

all: $(PROM)

basic.out: $(SOURCE) basic.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm $(PROM)