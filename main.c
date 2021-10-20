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
#include <sys/poll.h>
#include <fcntl.h>
#include <sys/queue.h>


void setnonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        return;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return;
}

typedef struct connfd_again {
    int fd;
    int idx;
    struct connfd_again *next;
} connfd_again_t;

struct fd_entry {
    int fd;
    STAILQ_ENTRY(fd_entry) entries;
};

STAILQ_HEAD(fdqueue, fd_entry);


int add_fd_channel_queue(channel_t *channel_arr, int idx, int connfd, int conn_loop_num)
{
    // LOGD("try to add fd %d\n", connfd);
    // sleep(5);
    // return -connfd;
    int err;
    channel_t *channel_ptr = &channel_arr[idx % conn_loop_num];
    err = pthread_mutex_trylock(&(channel_ptr->lock));
    if (err == EBUSY) {
        return -connfd;
    }
    if (err != 0)
        error("try lock");

    LOGD("lock in main\n");

    connection_t *conn_ptr = (connection_t *)calloc(1, sizeof(connection_t));
    conn_ptr->fd = connfd;

    if (channel_ptr->len == 0) {
        channel_ptr->head = conn_ptr;
        channel_ptr->tail = conn_ptr;
    }
    else {
        channel_ptr->tail->next = conn_ptr;
        channel_ptr->tail = conn_ptr;
    }
    channel_ptr->len ++;
    // idx++;
    pthread_mutex_unlock(&(channel_ptr->lock));
    LOGD("main ptr address %p, len %d\n",channel_ptr, channel_ptr->len);
    return 0;
    // return 1;
}

int main(int argc, char *argv[])
{
    char *port = "33333";
    struct sockaddr_storage cliaddr;
    socklen_t cliaddr_len;
    int listenfd, connfd;
    struct pollfd pfds[1];

    int numfds;
    int conn_loop_num = 2;
    int i, idx = 0;
    char buf[BUFSIZ];

    struct fdqueue fdqueue_head;
    STAILQ_INIT(&fdqueue_head);


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
        LOGD("channel address %p\n", job->args);
        /* because the job is infinite loop, so every thread only get one job */
        threadpool_add_job(tp, job);
    }

    /* poll */
    pfds[0].fd = listenfd;
    pfds[0].events = POLLIN;
    struct fd_entry *curr;

    while (1) {
        /* poll */
        errno = 0;
        numfds = poll(pfds, 1, 0);
        if (numfds < 0 && errno != EAGAIN) {
            error("poll");
        }

        // LOGD("numfds %d\n", numfds);

        for (i = 0; i< numfds; ++i) {
            /* check error */
            if (pfds[i].revents & (POLLERR | POLLNVAL))
                error("poll revents");

            errno = 0;
            connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);
            if (connfd < 0 && errno != EWOULDBLOCK) {
                    error("accept");
            }

            if (connfd >=0) {
                setnonblocking(connfd);
                LOGD("accept fd %d\n", connfd);

                // /* Add connfd to current channel */
                connfd = add_fd_channel_queue(channel_arr, idx, connfd, conn_loop_num);
                if (connfd < 0) {
                    LOGD("add to queue\n");
                    connfd = -connfd;
                    curr = (struct fd_entry *)malloc(sizeof(struct fd_entry));
                    curr->fd = connfd;
                    STAILQ_INSERT_TAIL(&fdqueue_head, curr, entries);
                }
                else
                    idx++;
            }
        }

        /* try to add residu connfd to channel */
        if (STAILQ_EMPTY(&fdqueue_head) == 0) {
            struct fdqueue fq_head_tmp;
            STAILQ_INIT(&fq_head_tmp);
            struct fd_entry *helper;

            curr = STAILQ_FIRST(&fdqueue_head);

            while (curr != NULL) {
                helper = STAILQ_NEXT(curr, entries);

                connfd = add_fd_channel_queue(channel_arr, idx, curr->fd, conn_loop_num);
                if (connfd < 0) {
                    connfd = -connfd;
                    curr->fd = connfd;
                    STAILQ_INSERT_TAIL(&fq_head_tmp, curr, entries);
                } else {
                    idx++;
                    free(curr);
                }
                curr = helper;
            }
            fdqueue_head = fq_head_tmp;
        }
    }

    return 0;
}