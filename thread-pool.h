#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <sys/time.h>

#define MAX_THREAD_NUM 16
#define DEFAULT_THREAD_NUM 4
#define MIN_THREAD_NUM 2

#define TIME_INTERVAL 0.5

/* ratio should >= 1.0 */
#define JOB_WORKER_RATIO 1.0

typedef struct job {
    void (*jobfun)(void *);
    void * args;
    struct job *next;
} job_t;

typedef struct worker {
    pthread_t thread;
    struct worker *next;
} worker_t;

typedef enum {
    fix_num = 0,
    dynamic_num,
} threadpool_dynamic_t; // ratio

typedef enum {
    no_shutdown = 0,
    shutdown_immediate,    /* immediate shutdown */
    shutdown_waitall,       /* after all jobs done */
} threadpool_shutdown_t;


typedef struct threadpool {
    pthread_mutex_t worker_lock;    /* only used in add and remove worker */
    pthread_mutex_t job_lock;
    pthread_cond_t notify;          /* job notify */

    int shutdown;                   
    int dynamic;

    int workernum;
    int target_workernum;           /* used in fix mode, to change threads num */
    time_t last_workerchange;
    worker_t *worker_head;

    int jobsnum;
    job_t *job_head;
    job_t *job_tail;

} threadpool_t;

typedef enum {
    threadpool_invalid = -2,
    threadpool_lock_failure = -1,
} threadpool_error_t;




threadpool_t *threadpool_init (int workernum, threadpool_dynamic_t dynamic);
int threadpool_add_job(threadpool_t *tp, job_t *job);
void threadpool_destory(threadpool_t *tp, threadpool_shutdown_t shutdown_type);
void threadpool_change_target_workernum(threadpool_t *tp, int target);

#define LOGD(...) fprintf(stderr, __VA_ARGS__)

#endif

