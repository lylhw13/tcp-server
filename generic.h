#ifndef GENERIC_H
#define GENERIC_H

#include <stdlib.h>
#include <stdio.h>

typedef struct connection
{
    int fd;
    struct connection *next;
} connection_t;

typedef struct channel
{
    pthread_mutex_t lock;
    // pthread_cond_t notify;

    int len;
    connection_t *head;
    connection_t *tail;
} channel_t;


static void error(const char *str)
{
    perror(str);
    exit(EXIT_FAILURE);
}

extern int create_and_bind(const char* port);


#endif