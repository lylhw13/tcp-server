#ifndef GENERIC_H
#define GENERIC_H

#include <stdlib.h>
#include <stdio.h>

static void error(const char *str)
{
    perror(str);
    exit(EXIT_FAILURE);
}

extern int create_and_bind(const char* port);

#endif