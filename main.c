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


void setnonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        return;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return;
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
                    int opt;
                    setnonblocking(connfd);

                   void *ptr;
                    if (ptr == NULL)
                        continue;

                    // memset(ptr, 0, sizeof(http_request_t));
                    // /* init session */
                    // ptr->fd = connfd;
                    // ptr->pos = ptr->buf;
                    // ptr->last = ptr->buf;

                    event.data.ptr = ptr;
                    event.events = EPOLLIN | EPOLLOUT;

                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &event) < 0)
                        error("epoll_ctl");

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