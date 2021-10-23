#include "generic.h"
#include <string.h>

/*
 * read
 * read a complete message
 * write
 * write complete 
 */

int read_cb(tcp_session_t *session)
{
    int nread;
    int fd = session->fd;
    char *buf = session->read_buf;
    struct epoll_event ev;

    errno = 0;
    nread = read(fd, buf, BUFSIZE);
    if (nread < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return nread;
        
        error("read in read_cb");    
    }

    if (nread == 0) 
        return nread;
    
    /* process request */
    fprintf(stdout, "thread %ld, read \n", (long)pthread_self());
    write(STDOUT_FILENO, buf, nread);
    return nread;
}

/* infinite loop */
void connect_cb(void *argus)
{
    LOGD("connect loop in thread %ld\n", (long)pthread_self());
    channel_t *channel_ptr = (channel_t *)argus;
    LOGD("thread %ld loop address %p\n", (long)pthread_self(), channel_ptr);
    int i, n;
    int epfd, fd;
    int nr_events;
    int nread, nwrite;
    connection_t *conn_ptr;
    tcp_session_t *session;
    server_t *curr_serv;
    on_read_complete_fun parse_message_cb = NULL;

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
            session = (tcp_session_t*)events[i].data.ptr;
            epfd = session->epfd;
            fd = session->fd;
            curr_serv = session->server;
            if (events[i].events & EPOLLIN) {
                /* normal read */
                if ((nread = read_cb(events[i].data.ptr)) == 0) {
                    struct epoll_event ev;
                    ev.data.ptr = events[i].data.ptr;
                    ev.events = events[i].events & ~EPOLLIN;
                    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0)
                        error("epoll_clt\n");
                }
                /* read_message_cb */
                parse_message_cb = curr_serv->read_complete_cb;
                if (parse_message_cb != NULL) {
                    int res;
                    res = parse_message_cb(session);
                    switch(res) {
                        case RCB_AGAIN:
                            if (session->read_pos < session->read_buf + BUFSIZE) 
                                break;
                            LOGD("TOO LONG MESSAGE\n");
                            /* fall through */
                        case RCB_ERROR:
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                            if (session->additional_info)
                                free(session->additional_info);
                            free(session);
                            break;
                        default:
                            break;
                    }
                }
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
        channel_ptr->head = conn_ptr->next;
        channel_ptr->len --;

        LOGD("loop get fd %d\n", conn_ptr->fd);
        pthread_mutex_unlock(&(channel_ptr->lock));

        /* add event */
        struct epoll_event ev;
        tcp_session_t* ptr = (tcp_session_t*)calloc(1, sizeof(tcp_session_t));

        if (curr_serv->add_info_size != 0) {
            ptr->add_info_size = curr_serv->add_info_size;
            ptr->additional_info = malloc(sizeof(ptr->add_info_size));
            memcpy(ptr->additional_info, curr_serv->additional_info, curr_serv->add_info_size);
        }

        ptr->fd = conn_ptr->fd;
        ptr->epfd = epfd;
        ev.data.ptr = ptr;
        ev.events = EPOLLIN | EPOLLOUT;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, ptr->fd, &ev) != 0)
            error("add fd to epoll in thread");
        free(conn_ptr);
    }     /* end while */
}