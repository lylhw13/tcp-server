#include "generic.h"
#include "chat.h"
#include <string.h>

#define MESSAGE_BEGIN 0
#define MESSAGE_ING 1

#define MESSAGE_ERROR       PARSE_ERROR
#define MESSAGE_OK          PARSE_OK
#define MESSAGE_PARTIAL     PARSE_AGAIN

// struct message *gen_message()
static struct message_queue message_queue_head;
static int total_msg_num;
static int *channel_msg_num;


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
        total_msg_num++;
        return MESSAGE_OK;
    }
    return MESSAGE_PARTIAL;
}

int on_write_message_complete(tcp_session_t *session)
{

}



int main(int argc, char *argv[])
{
    server_t *serv;
    char *host = "127.0.0.1";
    char *port = "33333";
    int conn_loop_num = 5;
    serv = server_init(host, port, conn_loop_num);
    serv->read_complete_cb = on_read_message_complete;


    STAILQ_INIT(&message_queue_head);   /* message queue for group */


    server_start(serv);
    server_run(serv);
    return 0;
}