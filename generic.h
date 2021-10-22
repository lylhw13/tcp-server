#ifndef GENERIC_H
#define GENERIC_H

#include "thread-pool.h"
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
    char read_buf[BUFSIZE];
    char *read_pos;     /* last read end pos */
    // size_t read_size;
    char *parse_pos;    /* last parse end pos */
    int state;

    /* for write */
    char *write_buf;
    char *wirte_pos;
    size_t write_size;
}tcp_session_t;

typedef int (*on_read_message_complete)(tcp_session_t *seesion);
typedef struct server {
    threadpool_t *tp;
    on_read_message_complete read_complete_ptr;

} server_t;

#endif