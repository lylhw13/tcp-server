#ifndef CHAT_H
#define CHAT_H

#include "generic.h"
#include <sys/queue.h>

#define MESSAGE_SIGNATURE 0x4D534753 /* "MSGS" */

struct message{
    unsigned int signature;
    unsigned int version;
    // STAILQ_ENTRY(message) entries;
    unsigned int length;
    // unsigned char body[0];
    unsigned char *body;
};
struct message_entry{
    struct message *ptr;
    STAILQ_ENTRY(message_entry) entries;
};

STAILQ_HEAD(message_queue, message);

#endif