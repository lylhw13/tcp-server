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


struct fd_entry {
    int fd;
    STAILQ_ENTRY(fd_entry) entries;
};
STAILQ_HEAD(fdqueue, fd_entry);


int add_fd_channel_queue(channel_t *channel_arr, int idx, int connfd, int conn_loop_num)
{
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
    pthread_mutex_unlock(&(channel_ptr->lock));
    LOGD("main ptr address %p, len %d\n",channel_ptr, channel_ptr->len);
    return 0;
}

server_t *server_init(const char* port, int conn_loop_num)
{
    int i, listenfd;
    server_t *serv;
    channel_t *channel_arr;
    threadpool_t *tp;

    serv = (server_t*)xmalloc(sizeof(server_t));
    memset(serv, 0, sizeof(server_t));
    serv->conn_loop_num = conn_loop_num;

    /* prepare listen socket */
    listenfd = create_and_bind(port);
    LOGD("listen fd %d\n", listenfd);
    if (listenfd < 0)
        error("create and bind");

    if (listen(listenfd, SOMAXCONN) < 0)
        error("listen");
    setnonblocking(listenfd);
    signal(SIGPIPE, SIG_IGN);
    serv->listenfd = listenfd;

    /* threadpool things */
    channel_arr = (channel_t *)calloc(conn_loop_num, sizeof(channel_t));
    if (channel_arr == NULL)
        error("calloc channel_arr");
    serv->channel_arr = channel_arr;
    for (i = 0; i< conn_loop_num; ++i)
        channel_arr[i].serv = serv;

    tp = threadpool_init(conn_loop_num, fix_num);
    if (tp == NULL)
        error("threadpool_init\n");
    serv->tp = tp;

    return serv;
}

void server_start(server_t *serv)
{
    serv->loop_state = LOOP_RUN;
}
void server_run(server_t * serv)
{
    int i, numfds, idx = 0;
    struct pollfd pfds[1];
    struct sockaddr_storage cliaddr;
    socklen_t cliaddr_len;
    int connfd;
    struct fdqueue fdqueue_head;
    STAILQ_INIT(&fdqueue_head);

    for (i = 0; i < serv->conn_loop_num; ++i)
    {
        job_t *job = (job_t *)xmalloc(sizeof(job_t *));

        job->jobfun = &connect_cb;
        job->args = &(serv->channel_arr[i]);
        LOGD("channel address %p\n", job->args);
        /* because the job is infinite loop, so every thread only get one job */
        threadpool_add_job(serv->tp, job);
    }

    /* poll */
    pfds[0].fd = serv->listenfd;
    pfds[0].events = POLLIN;
    struct fd_entry *curr;

    while (serv->loop_state) {
        /* poll */
        errno = 0;
        numfds = poll(pfds, 1, 0);
        if (numfds < 0 && errno != EAGAIN) {
            perror("poll");
            goto out;
        }

        for (i = 0; i< numfds; ++i) {
            /* check error */
            if (pfds[i].revents & (POLLERR | POLLNVAL)) {
                error("poll revents");
                goto out;
            }

            errno = 0;
            connfd = accept(serv->listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);
            if (connfd < 0 && errno != EWOULDBLOCK) {
                perror("accept");
                goto out;
            }

            if (connfd >=0) {
                setnonblocking(connfd);
                LOGD("accept fd %d\n", connfd);

                /* Add connfd to current channel */
                connfd = add_fd_channel_queue(serv->channel_arr, idx, connfd, serv->conn_loop_num);
                if (connfd < 0) {
                    LOGD("add to queue\n");
                    connfd = -connfd;
                    curr = (struct fd_entry *)xmalloc(sizeof(struct fd_entry));
                    curr->fd = connfd;
                    STAILQ_INSERT_TAIL(&fdqueue_head, curr, entries);
                }
                else
                    idx++;
            }
        }

        /* try to add residue connfd to channel */
        if (STAILQ_EMPTY(&fdqueue_head) == 0) {
            struct fdqueue fq_head_tmp;
            STAILQ_INIT(&fq_head_tmp);
            struct fd_entry *helper;

            curr = STAILQ_FIRST(&fdqueue_head);

            while (curr != NULL) {
                helper = STAILQ_NEXT(curr, entries);

                connfd = add_fd_channel_queue(serv->channel_arr, idx, curr->fd, serv->conn_loop_num);
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
out:
    server_destory(serv);
}

void server_stop(server_t *serv)
{
    serv->loop_state = LOOP_STOP;
}

void server_destory(server_t *serv)
{
    threadpool_destory(serv->tp, shutdown_waitall);
    free(serv->channel_arr);
    free(serv);
}