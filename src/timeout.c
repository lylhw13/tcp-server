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

RB_HEAD(event_tree, tcp_session) time_tree = RB_INITIALIZER(&time_tree);
RB_PROTOTYPE(event_tree, tcp_session, entry, compare);
RB_GENERATE(event_tree, tcp_session, entry, compare);


void timeout_insert(struct tcp_session *ts)
{
    struct tcp_session *tmp;
    tmp = RB_FIND(event_tree, &time_tree, ts);

    if (tmp != NULL) {
        struct timeval tv;
        struct timeval add={0,1};

        /* find a unique time, to keep every tcp_session */
        tv = ts->ev_timeout;
        do {
            timeradd(&tv, &add, &tv);
            tmp = RB_NEXT(event_tree, &time_tree, tmp);
        } while (tmp != NULL && timercmp(&tv, &(tmp->ev_timeout), ==));

        ts->ev_timeout = tv;
    }
    LOGD("add fd %d, at %ld, %ld\n", ts->fd, ts->ev_timeout.tv_sec, ts->ev_timeout.tv_usec);
    tmp = RB_INSERT(event_tree, &time_tree, ts);
    assert(tmp == NULL);
}

void timeout_process(void)
{
    struct timeval now;
    struct tcp_session *ts, *next;

    gettimeofday(&now, NULL);

    for (ts = RB_MIN(event_tree, &time_tree); ts; ts = next) {
        if (timercmp(&(ts->ev_timeout), &now, >=))
            break;

        next = RB_NEXT(event_tree, &time_tree, ts);

        LOGD("remove fd %d at %ld, %ld\n", ts->fd, ts->ev_timeout.tv_sec, ts->ev_timeout.tv_usec);
        RB_REMOVE(event_tree, &time_tree, ts);
        epoll_ctl(ts->epfd, EPOLL_CTL_DEL, ts->fd, NULL);
    }
}

void timeout_set(struct tcp_session *ts)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    timeradd(&now, &validity_period, &now);
    ts->ev_timeout = now;
}

void timeout_update(struct tcp_session *ts)
{
    RB_REMOVE(event_tree, &time_tree, ts);
    timeout_set(ts);
    timeout_insert(ts);
}