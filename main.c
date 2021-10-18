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

typedef struct {
    int fd;
    threadpool_t *tp;
    int index;
} conn_argument_t;

typedef struct {

} argument_t;

int connect_filter(void *argu)
{
    int *i = (int *)argu;
    return (*i)%2;
}

// void read_cb(void *argus)
// {
//     argument_t *argptr = (argument_t *)argus;
//     int fd = argptr->fd;
//     int nread;
//     char *buf;
//     printf("hello %d\n", fd);
//     buf = (char *)malloc(BUFSIZE);
//     while (1) {
//         errno = 0;
//         nread = read(fd, buf, BUFSIZE);
//         if (nread < 0)
//             if (errno == EAGAIN || errno == EWOULDBLOCK)
//                 continue;
//         if (nread <= 0)
//             break;
        
//         fprintf(stdout, "thread %ld, read \n", (long)pthread_self());
//         write(STDOUT_FILENO, buf, nread);
//     }
//     printf("end\n");
// }

typedef struct connection {
    int fd;
    struct connection *next;
} connection_t;


void connect_cb(void *argus)
{

    conn_argument_t *argptr = (conn_argument_t *)argus;
    int fd = argptr->fd;
    int i, n;
    int epfd;
    int maxevents;

    epfd = epoll_create1(0);
    if (epfd < 0)
        error("epoll_create1");

    while (1) {

    }

    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLOUT;

    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

    struct epoll_event *events;
    events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * maxevents);


    while (1) {
        n = epoll_wait(epfd, events, maxevents, 0);
        for (i = 0; i< n; ++i) {
            if (events[i].events & EPOLLIN) {

            }
        }
    }

}


/* single process */
int main(int argc, char *argv[])
{    
    char *port = "33333";
    int listenfd;
    int currfd;
    int epfd;
    int conn_loop_num = 2;

    struct sockaddr_storage cliaddr;
    socklen_t cliaddr_len;
    int connfd;
    struct epoll_event event, *events;
    int nr_events, i;
    int nread;
    char buf[BUFSIZ];

    listenfd = create_and_bind(port);
    LOGD("listen fd %d\n", listenfd);
    if (listenfd < 0) 
        error("create and bind");

    if (listen(listenfd, SOMAXCONN) < 0)
        error("listen");
    setnonblocking(listenfd);
    
    epfd = epoll_create1(0);
    if (epfd < 0)
        error("epoll_create1");


    /* Add listen to epoll */
    event.data.fd = listenfd;
    event.events = EPOLLIN | EPOLLOUT;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &event) < 0)
        error("epoll_ctl");

    events = malloc(sizeof (struct epoll_event) * MAX_EVENTS);
    if (!events)
        error("malloc");

    threadpool_t *tp;
    tp = threadpool_init(conn_loop_num, fix_num);
    if (tp == NULL)
        error("threadpool_init\n");

    typedef struct channel {
        pthread_mutex_t lock;
        pthread_cond_t notify;

        int len;
        connection_t *head;
        connection_t *tail;
    } channel_t;

    channel_t *channel_arr = (channel_t *)calloc(conn_loop_num, sizeof(channel_t));
    int idx = 0;
    int i;

    for (i = 0; i < conn_loop_num; ++i) {
        job_t *job = (job_t *)malloc(sizeof(job_t*));
        conn_argument_t *argu = (conn_argument_t *)malloc(sizeof(conn_argument_t));
        argu->fd = connfd;
        job->jobfun = &connect_cb;
        job->args = argu;

        threadpool_add_job(tp, job);
    }

    // here we only notify one listenfd
    struct pollfd pfds[1];
    pfds[1].fd = listenfd;
    pfds[1].events = POLLIN;
    

    while (1) {
        int connfd;
        int numfds;
        
        errno = 0;
        numfds = poll(pfds, 1, 0);
        if (numfds < 0) {
            if (errno == EAGAIN )
                continue;

            error("poll");
        }
        if (numfds == 0)    /* timeout */
            continue;

        if (pfds[0].revents & (POLLERR | POLLNVAL))
            error("poll revents");

        errno = 0;
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);
        if (connfd < 0) {
            if (errno == EWOULDBLOCK)
                continue;
            
            error("accept");
        }

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
        
        if (channel_ptr->head == NULL) {
            channel_ptr->head = conn_ptr;
            channel_ptr->tail = conn_ptr;
        }
        else {
            channel_ptr->tail->next = conn_ptr;
            channel_ptr->tail = conn_ptr;
        }
        idx ++;
    }



    while (1) {
        nr_events = epoll_wait(epfd, events, MAX_EVENTS, 0);
        if (nr_events < 0) {
            error("epoll_wait");
            free(events);
            return -1;
        }

        for (i = 0; i < nr_events; ++i) {
            if (events[i].data.fd == listenfd) {
                connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);   /* does this would block */

                // LOGD("listen: connect %d\n", connfd);
                if (connfd > 0) {
                    setnonblocking(connfd);

                    event.data.fd = connfd;
                    event.events = EPOLLIN | EPOLLOUT;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &event) < 0)
                        error("epoll_ctl");

                }
                continue;
            }
        }   /* end for */
    }   /* end while */

    free(events);
    return 0;
}