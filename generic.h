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
    struct server *serv;

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
    int read_pos;     /* last read end pos */
    int parse_pos;    /* last parse end pos */

    int parse_state;

    int message_offset;

    /* for write */
    char *write_buf;
    int write_pos;
    int write_size;
    int write_buf_free_flag;    /* wheter need to free */
    struct server *server;

    void *additional_info;
    int add_info_size;
}tcp_session_t;

typedef int (*on_read_complete_fun)(tcp_session_t *seesion);
typedef int (*on_write_complete_fun)(tcp_session_t *seesion);

/* return value for on_read_message_fun */
#define RCB_ERROR -1
#define RCB_OK 0
#define RCB_NEED_MORE 1
#define RCB_AGAIN 2

#define WCB_ERROR -1
#define WCB_OK 0
#define WCB_AGAIN 1

#define LOOP_RUN 1
#define LOOP_STOP 0

typedef struct server {
    int listenfd;
    int loop_state;
    int conn_loop_num;
    threadpool_t *tp;
    channel_t *channel_arr;

    on_read_complete_fun read_complete_cb;
    on_write_complete_fun write_complete_cb;

    void *additional_info;
    int add_info_size;
} server_t;

extern server_t *server_init(const char*host, const char *port, int conn_loop_num);
extern void server_start(server_t *);
extern void server_run(server_t *);
extern void server_stop(server_t *);    /* anthoer thread may use this */
extern void server_destory(server_t *);

#endif