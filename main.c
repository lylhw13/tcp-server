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

typedef struct argument {
    int fd;
} argument_t;

void read_cb(void *argus)
{
    argument_t *argptr = (argument_t *)argus;
    int fd = argptr->fd;
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

void connect_cb(void *argus)
{
    
}

void write_cb(void *argus)
{
    argument_t *argptr = (argument_t *)argus;
    int fd = argptr->fd;
    fprintf(stdout, "thread %ld, write hello world\n", (long)pthread_self());
}


/* single process */
int main(int argc, char *argv[])
{    
    char *port = "33333";
    int listenfd;
    int currfd;
    int epfd;

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
    tp = threadpool_init(2, fix_num);
    if (tp == NULL)
        error("threadpool_init\n");

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

                    job_t *job = (job_t *)malloc(sizeof(job_t*));
                    argument_t *argu = (argument_t *)malloc(sizeof(argument_t));
                    argu->fd = connfd;
                    job->jobfun = &read_cb;
                    job->args = argu;

                    threadpool_add_job(tp, job);

                }
                continue;
            }
            else {
                /* producer and consumer */
                if (events[i].events & EPOLLIN) {

                    // do_request(events[i].data.ptr);
                }

                if (events[i].events & EPOLLOUT) {

                    // do_respond(events[i].data.ptr);
                }
            }
        }   /* end for */
    }   /* end while */

    free(events);
    return 0;
}