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
    char *parse_pos;    /* last parse end pos */
    int state;

    /* for write */
    char *write_buf;
    char *wirte_pos;
    size_t write_size;
    server_t *server;
}tcp_session_t;

typedef int (*on_read_message_fun)(tcp_session_t *seesion);
typedef int (*on_write_message_fun)(tcp_session_t *seesion);

/* return value for on_read_message_fun */
#define PARSE_ERROR -1
#define PARSE_OK 0
#define PARSE_AGAIN 1

typedef struct server {
    threadpool_t *tp;
    int loop_state;

    on_read_message_fun read_complete_cb;
    on_write_message_fun write_complete_cb;
} server_t;

extern server_t *server_init(const char*host, const char *port);
extern void server_start(server_t *);
extern void server_run(server_t *);
extern void server_stop(server_t *);
extern void server_destory(server_t *);

#endif