#ifndef CHAT_H
#define CHAT_H

// #include "generic.h"
#include <sys/queue.h>
#include <stddef.h>

#define MESSAGE_SIGNATURE 0x4D534753 /* "MSGS" */
#define MESSAGE_VERSION 1.0

struct message{
    unsigned int signature;
    unsigned int version;
    unsigned int length;
    unsigned char body[0];
};
struct message_entry{
    struct message *ptr;
    STAILQ_ENTRY(message_entry) entries;
};

STAILQ_HEAD(message_queue, message);

#define msg_entry_size(len) ((offsetof(struct message, body) + len + 8) & ~7)

#endif