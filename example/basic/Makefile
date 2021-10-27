INC_DIR = ../../
CFLAGS = -g -Wall -I$(INC_DIR)
CC = gcc

LIBS = -lpthread

SRCDIR = ../../
# VPATH = $(SRCDIR)
SOURCE = $(wildcard  $(SRCDIR)*.c)
# $(info var in $(SOURCE))

PROM = basic.out

all: $(PROM)

basic.out: $(SOURCE) basic.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
	rm $(PROM)