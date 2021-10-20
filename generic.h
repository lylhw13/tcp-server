#ifndef GENERIC_H
#define GENERIC_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_EVENTS 64
#define BUFSIZE 1024

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
extern void connect_cb(void *argus);

#define LOGD(...) fprintf(stderr, __VA_ARGS__)

typedef struct tcp_session {
    int fd;
    int epfd;
    /* for read */
    char buf[BUFSIZE];
    char *read_pos;
    size_t read_size;

    /* for write */
    char *write_buf;
    char *wirte_pos;
    size_t write_size;
}tcp_session_t;

#endif