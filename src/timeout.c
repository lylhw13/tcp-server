#include "generic.h"
#include "tree.h"

#include <sys/time.h>
#include <assert.h>

static struct timeval validity_period = {50,0}; /* seconds */

int compare(struct tcp_session *t1, struct tcp_session *t2)
{
    if (timercmp(&(t1->ev_timeout), &(t2->ev_timeout), <))
        return -1;
    else if (timercmp(&(t1->ev_timeout), &(t2->ev_timeout), >))
        return 1;

    return 0;
}

RB_PROTOTYPE(event_tree, tcp_session, entry, compare);
RB_GENERATE(event_tree, tcp_session, entry, compare);

void timeout_insert(struct event_tree *head, struct tcp_session *ts)
{
    LOGD("%s\n", __FUNCTION__);
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
    tmp = RB_INSERT(event_tree, head, ts);
    assert(tmp == NULL);

// #ifdef DEBUG
    struct timeval now;
    gettimeofday(&now, NULL);
    LOGD("head %p add fd %d, val %ld, %ld, at %ld, %ld \n", head, ts->fd, ts->ev_timeout.tv_sec, ts->ev_timeout.tv_usec, now.tv_sec, now.tv_usec);
// #endif
}

void timeout_process(struct event_tree *head, void(*funcb)(struct event_tree *, struct tcp_session *))
{
    struct timeval now;
    struct tcp_session *ts, *next;
    
    gettimeofday(&now, NULL);

    for(ts = RB_MIN(event_tree, head); ts; ts = next) {
        if (timercmp(&(ts->ev_timeout), &now, >))
            break;
        
        next = RB_NEXT(event_tree, head, ts);
        funcb(head, ts);
    }
}

void timeout_remove(struct event_tree *head, struct tcp_session *ts)
{
    LOGD("%s\n", __FUNCTION__);

    struct tcp_session *tmp;
    tmp = RB_FIND(event_tree, head, ts);
    if (tmp != NULL)
        RB_REMOVE(event_tree, head, ts);
    
// #ifdef DEBUG
    struct timeval now;
    gettimeofday(&now, NULL);
    LOGD("head %p remove fd %d, val %ld %ld at %ld %ld\n", head, ts->fd, ts->ev_timeout.tv_sec, ts->ev_timeout.tv_usec, now.tv_sec, now.tv_usec);
// #endif
}

void timeout_set(struct event_tree *head, struct tcp_session *ts )
{
    struct timeval now;
    gettimeofday(&now, NULL);
    timeradd(&now, &validity_period, &now);
    ts->ev_timeout = now;
}

void timeout_update(struct event_tree *head, struct tcp_session *ts)
{
    // LOGD("%s\n", __FUNCTION__);

    // RB_REMOVE(event_tree, head, ts);
    timeout_remove(head, ts);
    timeout_set(head, ts);
    timeout_insert(head, ts);
}