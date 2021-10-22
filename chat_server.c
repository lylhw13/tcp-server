#include "generic.h"
#include "chat.h"
#include <string.h>

#define MESSAGE_BEGIN 0
#define MESSAGE_ING 1

#define MESSAGE_ERROR -1
#define MESSAGE_OK 0
#define MESSAGE_PARTIAL 1

// struct message *gen_message()
static struct message_queue message_queue_head;
static int msg_num;


int on_read_message_complete(tcp_session_t *session)
{
    struct message *msg_ptr;// = (struct message*)malloc(struct message);
    int length;
    msg_ptr = (struct message *)session->read_buf;
    if (msg_ptr->signature != MESSAGE_SIGNATURE) {
        fprintf(stderr, "error message signature\n");
        return MESSAGE_ERROR;
    }

    if (msg_ptr->version != MESSAGE_VERSION) {
        fprintf(stderr, "error message version\n");
        return MESSAGE_ERROR;
    }

    length = msg_ptr->length;
    if (length <= (session->read_pos - session->parse_pos)) {
        struct message *ptr = (struct message *)malloc(msg_entry_size(length));
        ptr->signature = MESSAGE_SIGNATURE;
        ptr->version = MESSAGE_VERSION;
        ptr->length = length;
        memncpy(ptr->body, session->parse_pos, length);
        struct message_entry *msg_entry = (struct message *)malloc(sizeof(struct message_entry));
        msg_entry->ptr = ptr;
        STAILQ_INSERT_TAIL(&message_queue_head, msg_entry, entries);
        msg_num++;
        return MESSAGE_OK;
    }
    return MESSAGE_PARTIAL;
}



int main(int argc, char *argv[])
{
    STAILQ_INIT(&message_queue_head);

    return 0;
}