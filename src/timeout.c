#include "generic.h"
#include "tree.h"

#include <sys/time.h>

int compare(struct tcp_session *t1, struct tcp_session *t2)
{
    if (timercmp(&(t1->ev_timeout), &(t2->ev_timeout), <))
        return -1;
    else if (timercmp(&(t1->ev_timeout), &(t2->ev_timeout), >))
        return 1;

    return 0;
}

void timeout_insert(struct tcp_session *ts, struct event_tree *head)
{
    struct tcp_session *tmp;
    tmp = RB_FIND(event_tree, head, ts);
    
    if (tmp != NULL) {
        struct timeval tv;
        struct timeval add={0,1};

        tv = ts->ev_timeout;
        do {
            timeradd(&tv, &add, &tv);
            tmp = RB_NEXT(event_tree, head, tmp);
        } while (tmp != NULL && timercmp(&tv, &(tmp->ev_timeout), ==));

        ts->ev_timeout = tv;
    }
    LOGD("add fd %d, at %ld, %ld\n", ts->fd, ts->ev_timeout.tv_sec, ts->ev_timeout.tv_usec);
    tmp = RB_INSERT(event_tree, head, ts);
    assert(tmp == NULL);
}

void timeout_process(struct event_tree *head, void(*funcb)(void *,void *))
{
    struct timeval now;
    struct tcp_session *ts, *next;
    
    gettimeofday(&now, NULL);

    for(ts = RB_MIN(event_tree, head); ts; ts = next) {
        if (timercmp(&(ts->ev_timeout), &now, >))
            break;
        
        next = RB_NEXT(event_tree, head, ts);
        LOGD("remove fd %d at %ld %ld\n", ts->fd, ts->ev_timeout.tv_sec, ts->ev_timeout.tv_usec);
        // RB_REMOVE(event_tree, head, ts);
        // epoll_ctl(ts->epfd, EPOLL_CTL_DEL, ts->fd, NULL);
        funcb(ts, head);
    }
}

void timeout_set(struct tcp_session *ts, struct event_tree *head)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    timeradd(&now, &validity_period, &now);
    ts->ev_timeout = now;
}

void timeout_update(struct tcp_session *ts, struct event_tree *head)
{
    RB_REMOVE(event_tree, head, ts);
    timeout_set(ts, head);
    timeout_insert(ts, head);
}