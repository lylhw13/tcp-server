#include "generic.h"
#include <string.h>

/*
 * read
 * read a complete message
 * write
 * write complete 
 */

tcp_session_t * create_session(int fd, int epfd, server_t *serv)
{
    tcp_session_t* session = (tcp_session_t*)calloc(1, sizeof(tcp_session_t));
    session->fd = fd;
    session->epfd = epfd;
    session->server = serv;
    session->read_pos = session->read_buf;
    session->parse_pos = session->read_buf;

    if (serv->add_info_size != 0) {
        session->add_info_size = serv->add_info_size;
        session->additional_info = calloc(1, sizeof(session->additional_info));
        memcpy(session->additional_info, serv->additional_info, serv->add_info_size);
    }
    return session;
}

void free_session(tcp_session_t *session)
{
    LOGD("%s\n", __FUNCTION__);
    if (session->additional_info)
        free(session->additional_info);
    free(session);
}

int read_cb(tcp_session_t *session)
{
    // LOGD("%s\n", __FUNCTION__);
    int nread;
    int fd = session->fd;
    char *buf = session->read_buf;
    struct epoll_event ev;

    errno = 0;
    nread = read(fd, session->read_pos, session->read_buf + BUFSIZE - session->read_pos);
    if (nread < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return nread;
        
        error("read in read_cb");    
    }

    if (nread == 0) 
        return nread;

    session->read_pos += nread;
    /* process request */
    // fprintf(stdout, "thread %ld, read \n", (long)pthread_self());
    write(STDOUT_FILENO, buf, nread);
    return nread;
}
void write_cb(tcp_session_t* session)
{
    // LOGD("%s\n", __FUNCTION__);
    int nwrite;
    int length;
    int fd = session->fd;

    if (session->write_buf == NULL || session->write_pos == NULL)
        return ;

    length = session->write_buf + session->write_size - session->write_pos;
    
    errno = 0;
    nwrite = write(fd, session->write_pos, length);
    if (nwrite < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        error("write in write_cb\n");
    }
    session->write_pos += nwrite;
    // LOGD("end %s\n", __FUNCTION__);
    return ;
}

/* infinite loop */
void connect_cb(void *argus)
{
    channel_t *channel_ptr = (channel_t *)argus;
    LOGD("thread %ld loop address %p\n", (long)pthread_self(), channel_ptr);
    int i, n, res, nread;
    int epfd, fd;
    int nr_events;
    connection_t *conn_ptr;
    tcp_session_t *session;
    server_t *serv;
    on_read_complete_fun parse_message_cb;
    on_write_complete_fun write_message_cb;
    struct epoll_event ev;

    serv = channel_ptr->serv;
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
            fd = session->fd;
            if (events[i].events & EPOLLIN) {
                /* normal read */
                if ((nread = read_cb(events[i].data.ptr)) == 0) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    free_session(session);
                }
                /* read_message_cb */
                parse_message_cb = serv->read_complete_cb;
                if (parse_message_cb != NULL) {
                    
                    res = parse_message_cb(session);
                    switch(res) {
                        case RCB_AGAIN:
                            if (session->read_pos < session->read_buf + BUFSIZE) 
                                break;
                            LOGD("TOO LONG MESSAGE\n");
                            /* fall through */
                        case RCB_ERROR:
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                            free_session(session);
                            break;
                        default:
                            break;
                    }
                }
            }
            if (events[i].events & EPOLLOUT) {
                /* normal write */
                write_cb(session);
                write_message_cb = serv->write_complete_cb;
                if (write_message_cb != NULL) {
                    res = write_message_cb(session);
                    if (res == WCB_ERROR) {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        free_session(session);
                    }
                }
            }
        } /* end for */


        if (channel_ptr->len == 0)
            continue;

        int err;
        err = pthread_mutex_trylock(&(channel_ptr->lock));
        LOGD("lock in thread\n");
        LOGD("address %p, len %d\n", channel_ptr, channel_ptr->len);
        // sleep(5);
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

        tcp_session_t* session_new = create_session(conn_ptr->fd, epfd, serv);
        ev.data.ptr = session_new;
        ev.events = EPOLLIN | EPOLLOUT;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, session_new->fd, &ev) != 0)
            error("add fd to epoll in thread");
        free(conn_ptr);
        LOGD("add epoll\n");
    }     /* end while */
}

