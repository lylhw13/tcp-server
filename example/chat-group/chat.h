#ifndef CHAT_H
#define CHAT_H

#include <sys/queue.h>
#include <stddef.h>
#include <pthread.h>

#define MESSAGE_SIGNATURE 0x4D534753 /* "MSGS" */

struct message{
    unsigned int signature;
    unsigned int author;
    struct message_entry *ptr;
    unsigned int length;
    unsigned char body[0];
};

struct message_entry{
    struct message *ptr;
    STAILQ_ENTRY(message_entry) entries;
};

STAILQ_HEAD(message_queue, message_entry);

typedef struct chat_messages_queue {
    pthread_mutex_t *lock;
    struct message_queue *message_queue_head;
    int msg_offset;
    int *msg_total_num;
}chat_messages_queue_t;

#define msg_size_by_len(len) (offsetof(struct message, body) + len)
#define msg_size(msg) msg_size_by_len((msg)->length)

extern void readwrite(int sockfd);

#endif