#include "generic.h"
#include "tree.h"
#include <sys/time.h>
#include <string.h>

/*
 * read
 * read complete 
 * write
 * write complete 
 */

int compare(struct tcp_session *t1, struct tcp_session *t2)
{
    if (timercmp(&(t1->ev_timeout), &(t2->ev_timeout), <))
        return -1;
    else if (timercmp(&(t1->ev_timeout), &(t2->ev_timeout), >))
        return 1;

    return 0;
}

RB_HEAD(event_tree, tcp_session) time_tree = RB_INITIALIZER(&time_tree);
RB_PROTOTYPE(event_tree, tcp_session, entry, compare);
RB_GENERATE(event_tree, tcp_session, entry, compare);

tcp_session_t * create_session(int fd, int epfd, server_t *serv)
{
    tcp_session_t* session = (tcp_session_t*)calloc(1, sizeof(tcp_session_t));
    session->fd = fd;
    session->epfd = epfd;
    session->server = serv;

    if (serv->add_info_size != 0) {
        session->add_info_size = serv->add_info_size;
        session->additional_info = calloc(1, session->add_info_size);
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

void remove_session(tcp_session_t *session, struct event_tree *head)
{
    RB_REMOVE(event_tree, head, session);
    epoll_ctl(session->epfd, EPOLL_CTL_DEL, session->fd, NULL);
    free_session(session);
}

int read_cb(tcp_session_t *session)
{
    // LOGD("%s\n", __FUNCTION__);
    int nread;
    int fd = session->fd;
    char *buf = session->read_buf;

    errno = 0;
    nread = read(fd, buf + session->read_pos, BUFSIZE - session->read_pos);
    if (nread < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return nread;
        
        error("read in read_cb");    
    }

    if (nread == 0) 
        return nread;

    session->read_pos += nread;
    return nread;
}
void write_cb(tcp_session_t* session)
{
    // LOGD("%s\n", __FUNCTION__);
    int nwrite;
    int length;
    int fd = session->fd;

    if (session->write_buf == NULL)
        return ;

    length = session->write_size - session->write_pos;
    if (length == 0)
        return;
    errno = 0;
    nwrite = write(fd, session->write_buf + session->write_pos, length);
    if (nwrite < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        error("write in write_cb\n");
    }

    session->write_pos += nwrite;
    return ;
}

/* infinite loop */
void connect_cb(void *argus)
{
    channel_t *channel_ptr = (channel_t *)argus;
    // LOGD("thread %ld loop address %p\n", (long)pthread_self(), channel_ptr);
    int i, res, nread;
    int epfd, fd;
    int nr_events;
    connection_t *conn_ptr;
    tcp_session_t *session;
    server_t *serv;
    on_read_complete_fun parse_message_cb;
    on_write_complete_fun write_message_cb;
    struct epoll_event ev;
    static struct timeval event_tv;

    RB_HEAD(event_tree, tcp_session) head;
    RB_INIT(&head);

    serv = channel_ptr->serv;
    epfd = epoll_create1(0);
    if (epfd < 0)
        error("epoll_create1");

    struct epoll_event *events;
    events = (struct epoll_event *)xmalloc(sizeof(struct epoll_event) * MAX_EVENTS);

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
                    // epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    // free_session(session);
                    remove_session(session, &head);
                    continue;
                }
                /* read_message_cb */
                parse_message_cb = serv->read_complete_cb;
                if (parse_message_cb != NULL) {
                    res = parse_message_cb(session);
                    switch(res) {
                        case RCB_AGAIN:
                            if (session->read_pos < BUFSIZE) 
                                break;
                            LOGD("TOO LONG MESSAGE\n");
                            /* fall through */
                        case RCB_ERROR:
                            // epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                            // free_session(session);
                            remove_session(session, &head);
                            continue;
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
                        // epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        // free_session(session);
                        remove_session(session, &head);
                        continue;
                    }
                }
            }
            timeout_update(session, &head);
        } /* end for */

        timeout_process(&head, remove_session);

        if (channel_ptr->len == 0)
            continue;

        int err;
        err = pthread_mutex_trylock(&(channel_ptr->lock));
        LOGD("lock in thread\n");
        LOGD("address %p, len %d\n", channel_ptr, channel_ptr->len);
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

        tcp_session_t* session_new = create_session(conn_ptr->fd, epfd, serv);
        ev.data.ptr = session_new;
        ev.events = EPOLLIN | EPOLLOUT;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, session_new->fd, &ev) != 0)
            error("add fd to epoll in thread");

        timeout_insert(session_new, &head);
        free(conn_ptr);
    }     /* end while */
}

