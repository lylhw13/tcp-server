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

static tcp_session_t * create_session(int fd, int epfd, server_t *serv)
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

static void free_session(tcp_session_t *session)
{
    LOGD("%s\n", __FUNCTION__);
    if (session->additional_info)
        free(session->additional_info);
    free(session);
}

static void remove_session(struct event_tree *head, tcp_session_t *session)
{
    close(session->fd);
    timeout_remove(head, session);
    epoll_ctl(session->epfd, EPOLL_CTL_DEL, session->fd, NULL);
    free_session(session);
}

static int read_cb(tcp_session_t *session)
{
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
static int write_cb(tcp_session_t* session)
{
    int nwrite;
    int length;
    int fd = session->fd;

    if (session->write_buf == NULL)
        return 0;

    length = session->write_size - session->write_pos;
    if (length == 0)
        return 0;
    errno = 0;
    nwrite = write(fd, session->write_buf + session->write_pos, length);
    if (nwrite < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        error("write in write_cb\n");
    }

    session->write_pos += nwrite;
    return nwrite;
}

static void add_new_session(int epfd, channel_t *channel_ptr, struct event_tree *head)
{
    int err;
    connection_t *conn_ptr;
    struct epoll_event ev;

    err = pthread_mutex_trylock(&(channel_ptr->lock));
    LOGD("lock in thread\n");
    LOGD("address %p, len %d\n", channel_ptr, channel_ptr->len);
    if (err == EBUSY)
        return;
    if (err != 0)
        error("try lock in thread");
    if (channel_ptr->len == 0) {
        pthread_mutex_unlock(&(channel_ptr->lock));
        return;
    }
    conn_ptr = channel_ptr->head;
    channel_ptr->head = conn_ptr->next;
    channel_ptr->len --;

    LOGD("loop get fd %d\n", conn_ptr->fd);
    pthread_mutex_unlock(&(channel_ptr->lock));

    tcp_session_t* session_new = create_session(conn_ptr->fd, epfd, channel_ptr->serv);
    ev.data.ptr = session_new;
    ev.events = EPOLLIN | EPOLLOUT;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, session_new->fd, &ev) != 0)
        error("add fd to epoll in thread");

    timeout_set(head, session_new);
    timeout_insert(head, session_new);
    free(conn_ptr);
}


/* infinite loop */
void connect_cb(void *argus)
{
    channel_t *channel_ptr = (channel_t *)argus;
    int i, res;
    int epfd, nr_events;
    int nread, nwrite,read_write_state = 0;
    tcp_session_t *session;
    server_t *serv;
    on_read_complete_fun parse_message_cb;
    on_write_complete_fun write_message_cb;
    struct event_tree head;

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
            read_write_state = 0;

            session = (tcp_session_t*)events[i].data.ptr;
            if (events[i].events & EPOLLIN) {
                /* normal read */
                if ((nread = read_cb(events[i].data.ptr)) == 0) {
                    remove_session(&head, session);
                    continue;
                }

                if (nread > 0)
                    read_write_state = 1;

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
                            remove_session(&head, session);
                            continue;
                        default:
                            break;
                    }
                }
            }
            if (events[i].events & EPOLLOUT) {
                /* normal write */
                nwrite = write_cb(session);
                write_message_cb = serv->write_complete_cb;
                if (write_message_cb != NULL) {
                    res = write_message_cb(session);
                    if (res == WCB_ERROR) {
                        remove_session(&head, session);
                        continue;
                    }
                }
                if (nwrite > 0)
                    read_write_state = 1;
            }
            if (read_write_state)
                timeout_update(&head, session);
        } /* end for */

        timeout_process(&head, remove_session);

        if (channel_ptr->len > 0)
            add_new_session(epfd, channel_ptr, &head);
    }     /* end while */
}

