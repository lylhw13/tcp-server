#include "generic.h"

void read_cb(int fd)
{
    // argument_t *argptr = (argument_t *)argus;
    // int fd = argptr->fd;
    int nread;
    char *buf;
    printf("hello %d\n", fd);
    buf = (char *)malloc(BUFSIZE);
    // while (1) {
        errno = 0;
        nread = read(fd, buf, BUFSIZE);
        // if (nread < 0)
        //     if (errno == EAGAIN || errno == EWOULDBLOCK)
        //         continue;
        // if (nread <= 0)
        //     break;

        fprintf(stdout, "thread %ld, read \n", (long)pthread_self());
        write(STDOUT_FILENO, buf, nread);
    // }
    printf("end\n");
}

/* infinite loop */
void connect_cb(void *argus)
{
    LOGD("connect loop in thread %ld\n", (long)pthread_self());
    channel_t *channel_ptr = (channel_t *)argus;
    LOGD("thread %ld loop address %p\n", (long)pthread_self(), channel_ptr);
    int i, n;
    int epfd;
    int nr_events;
    connection_t *conn_ptr;

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
                LOGD("read cb\n");
                read_cb(events[i].data.fd);   /* read cb */
            }
            if (events[i].events & EPOLLOUT) {
                ; /* write cb */
            }
        } /* end for */

        if (channel_ptr->len == 0)
            continue;

        int err;
        err = pthread_mutex_trylock(&(channel_ptr->lock));
        LOGD("lock in thread\n");
        LOGD("address %p, len %d\n", channel_ptr, channel_ptr->len);
        // sleep(5);
        // LOGD("error %d\n", err);
        if (err == EBUSY)
            continue;
        if (err != 0)
            error("try lock in thread");
        if (channel_ptr->len == 0) {
            pthread_mutex_unlock(&(channel_ptr->lock));
            continue;
        }
        conn_ptr = channel_ptr->head;
        // LOGD("ptr len %d\n", channel_ptr->len);

        channel_ptr->head = conn_ptr->next;
        channel_ptr->len --;

        LOGD("loop get fd %d\n", conn_ptr->fd);
        pthread_mutex_unlock(&(channel_ptr->lock));

        struct epoll_event ev;
        ev.data.fd = conn_ptr->fd;
        ev.events = EPOLLIN | EPOLLOUT;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_ptr->fd, &ev) != 0)
            error("add fd to epoll in thread");

        free(conn_ptr);

    }     /* end while */
}