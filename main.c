#include "generic.h"
#include "thread-pool.h"

#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_EVENTS 64
#define BUFSIZE 1024

void setnonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        return;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return;
}

int connect_filter(void *argu)
{
    int *i = (int *)argu;
    return (*i) % 2;
}

void read_cb(int fd)
{
    // argument_t *argptr = (argument_t *)argus;
    // int fd = argptr->fd;
    int nread;
    char *buf;
    printf("hello %d\n", fd);
    buf = (char *)malloc(BUFSIZE);
    while (1) {
        errno = 0;
        nread = read(fd, buf, BUFSIZE);
        if (nread < 0)
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
        if (nread <= 0)
            break;

        fprintf(stdout, "thread %ld, read \n", (long)pthread_self());
        write(STDOUT_FILENO, buf, nread);
    }
    printf("end\n");
}

typedef struct connection
{
    int fd;
    struct connection *next;
} connection_t;

typedef struct channel
{
    pthread_mutex_t lock;
    pthread_cond_t notify;

    int len;
    connection_t *head;
    connection_t *tail;
} channel_t;

// typedef struct
// {
//     // int index;
//     channel_t *ptr;
//     // threadpool_t *tp;
// } conn_argument_t;

void connect_cb(void *argus)
{
    // conn_argument_t *argptr = (conn_argument_t *)argus;
    channel_t *channel_ptr = (channel_t *)argus;
    int fd;
    int i, n;
    int epfd;
    int nr_events;

    epfd = epoll_create1(0);
    if (epfd < 0)
        error("epoll_create1");

    struct epoll_event *events;
    events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * MAX_EVENTS);

    while (1)
    {
        nr_events = epoll_wait(epfd, events, MAX_EVENTS, 0);
        if (nr_events < 0) {
            free(events);
            error("epoll_wait");
        }

        for (i = 0; i < nr_events; ++i) {
            if (events[i].events & EPOLLIN) {
                read_cb(events[i].data.fd);   /* read cb */
            }
            if (events[i].events & EPOLLOUT) {
                ; /* write cb */
            }
        } /* end for */

        int err;
        err = pthread_mutex_trylock(&(channel_ptr->lock));
        if (err == EBUSY)
            continue;
        if (err != 0)
            error("try lock in thread");
        connection_t *conn_ptr;
        if (channel_ptr->len == 0) {
            pthread_mutex_unlock(&(channel_ptr->lock));
            continue;
        }

        conn_ptr = channel_ptr->head;
        channel_ptr->head = conn_ptr->next;
        channel_ptr->len --;
        
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = EPOLLIN | EPOLLOUT;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0)
            error("add fd to epoll in thread");

        free(conn_ptr);

    }     /* end while */
}

/* single process */
int main(int argc, char *argv[])
{
    char *port = "33333";
    int listenfd, connfd;
    int numfds;
    int conn_loop_num = 2;
    int i, idx = 0;

    struct sockaddr_storage cliaddr;
    socklen_t cliaddr_len;
    char buf[BUFSIZ];

    /* prepare listen socket */
    listenfd = create_and_bind(port);
    LOGD("listen fd %d\n", listenfd);
    if (listenfd < 0)
        error("create and bind");

    if (listen(listenfd, SOMAXCONN) < 0)
        error("listen");
    setnonblocking(listenfd);

    /* threadpool things */
    channel_t *channel_arr = (channel_t *)calloc(conn_loop_num, sizeof(channel_t));
    if (channel_arr == NULL)
        error("calloc channel_arr");

    threadpool_t *tp;
    tp = threadpool_init(conn_loop_num, fix_num);
    if (tp == NULL)
        error("threadpool_init\n");

    for (i = 0; i < conn_loop_num; ++i)
    {
        job_t *job = (job_t *)malloc(sizeof(job_t *));

        job->jobfun = &connect_cb;
        job->args = &channel_arr[i];
        threadpool_add_job(tp, job);
    }

    /* poll */
    struct pollfd pfds[1];
    pfds[1].fd = listenfd;
    pfds[1].events = POLLIN;

    while (1) {
        errno = 0;
        numfds = poll(pfds, 1, 0);
        if (numfds < 0)
        {
            if (errno == EAGAIN)
                continue;

            error("poll");
        }
        if (numfds == 0) /* timeout */
            continue;

        if (pfds[0].revents & (POLLERR | POLLNVAL))
            error("poll revents");

        errno = 0;
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);
        if (connfd < 0)
        {
            if (errno == EWOULDBLOCK)
                continue;

            error("accept");
        }

        setnonblocking(connfd);

        /* Add connfd to current channel */
        channel_t *channel_ptr = &channel_arr[idx % conn_loop_num];
        int err;
        err = pthread_mutex_trylock(&(channel_ptr->lock));
        if (err == EBUSY)
            continue;
        if (err != 0)
            error("try lock");

        connection_t *conn_ptr = (connection_t *)calloc(1, sizeof(connection_t));
        conn_ptr->fd = connfd;

        if (channel_ptr->head == NULL)
        {
            channel_ptr->head = conn_ptr;
            channel_ptr->tail = conn_ptr;
        }
        else
        {
            channel_ptr->tail->next = conn_ptr;
            channel_ptr->tail = conn_ptr;
        }
        channel_ptr->len ++;
        idx++;
    }

    return 0;
}