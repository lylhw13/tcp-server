INC_DIR = ../../src/

CFLAGS = -Wall -I$(INC_DIR)
CC = gcc

ifdef DEBUG
CFLAGS += -g -DDEBUG
endif

LIBS = -lpthread

SRCDIR = $(INC_DIR)
SOURCE = $(wildcard  $(SRCDIR)*.c)


PROM = basic.out

all: $(PROM)

basic.out: $(SOURCE) basic.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm $(PROM)