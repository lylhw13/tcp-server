#include "generic.h"
#include "chat.h"
#include <string.h>

#define MESSAGE_BEGIN 0
#define MESSAGE_ING 1

#define MESSAGE_ERROR       RCB_ERROR
#define MESSAGE_OK          RCB_OK
#define MESSAGE_PARTIAL     RCB_NEED_MORE
#define MESSAGE_LOCK_AGAIN  RCB_AGAIN

// struct message *gen_message()


int on_read_message_complete(tcp_session_t *session)
{
    int err, length;
    struct message *msg_begin;
    chat_messages_t *msg_info;

    msg_begin = (struct message *)session->read_buf;
    msg_info = (chat_messages_t*)session->additional_info;

    while (1) {
        if ((session->read_pos - session->parse_pos) < offsetof(struct message, body))
            return MESSAGE_PARTIAL;

        if (msg_begin->signature != MESSAGE_SIGNATURE) {
            fprintf(stderr, "error message signature\n");
            return MESSAGE_ERROR;
        }

        if (msg_begin->version != MESSAGE_VERSION) {
            fprintf(stderr, "error message version\n");
            return MESSAGE_ERROR;
        }

        length = msg_size_by_len(msg_begin->length);
        if (length <= (session->read_pos - session->parse_pos)) {
            struct message *ptr = (struct message *)malloc(length);
            memcpy(ptr, session->parse_pos, length);
            session->parse_pos += length;

            struct message_entry *msg_entry = (struct message_entry *)malloc(sizeof(struct message_entry));
            msg_entry->ptr = ptr;
            
            err = pthread_mutex_trylock(msg_info->lock);
            if (err == EBUSY)
                return MESSAGE_LOCK_AGAIN;
            if (err != 0)
                return MESSAGE_ERROR;
            STAILQ_INSERT_TAIL(msg_info->message_queue_head, msg_entry, entries);
            (*(msg_info->msg_total_num))++;
            pthread_mutex_unlock(msg_info->lock);

            /* shift buffer */
            memmove(session->read_buf, session->parse_pos, session->read_pos - session->parse_pos);
            return MESSAGE_OK;
        }
    }

    return MESSAGE_PARTIAL;
}

int on_write_message_complete(tcp_session_t *session)
{
    int err;
    struct message *msg;
    struct message_entry *msg_entry;
    struct chat_messages *msg_info;

    /* last write has not complete */
    if (session->write_pos < session->write_buf + session->write_size)
        return WCB_AGAIN;

    /* reset the write buffer */
    session->write_size = 0;
    session->write_pos = session->write_buf;


    msg_info = (chat_messages_t*)session->additional_info;
    err = pthread_mutex_lock(msg_info->lock);
    if (err == EBUSY)
        return WCB_AGAIN;
    if (err != 0)
        return WCB_ERROR;
    
    /* no more message */
    if (*(msg_info->msg_total_num) == 0 || 
        msg_info->msg_offset == *(msg_info->msg_total_num)) {
        pthread_mutex_unlock(msg_info->lock);
        return WCB_AGAIN;
    }

    if (session->write_buf == NULL) {
        if (msg_info->msg_offset != 0) {
            pthread_mutex_unlock(msg_info->lock);
            return WCB_ERROR;
        }
        
        msg_entry = STAILQ_FIRST(msg_info->message_queue_head);
    }
    else {
        msg_entry = (struct message_entry *)session->write_buf;
        msg_entry = STAILQ_NEXT(msg_entry, entries);
        if (msg_entry == NULL) {
            pthread_mutex_unlock(msg_info->lock);
            return WCB_AGAIN;
        }
    }

    // msg = msg_entry;
    session->write_buf = (char *)msg_entry->ptr;
    session->write_pos = session->write_buf;
    session->write_size = msg_size(msg_entry->ptr);
    pthread_mutex_unlock(msg_info->lock);
    return WCB_OK;
}



int main(int argc, char *argv[])
{
    server_t *serv;
    char *host = "127.0.0.1";
    char *port = "33333";
    int conn_loop_num = 5;
    chat_messages_t chat_msgs;

    pthread_mutex_t lock;
    struct message_queue message_queue_head;
    int msg_total_num;


    serv = server_init(host, port, conn_loop_num);
    STAILQ_INIT(&message_queue_head);   /* message queue for group */

    serv->read_complete_cb = on_read_message_complete;

    chat_msgs.lock = &lock;
    chat_msgs.message_queue_head = &message_queue_head;
    chat_msgs.msg_total_num = &msg_total_num;
    chat_msgs.msg_offset = 0;

    serv->additional_info = &chat_msgs;
    serv->add_info_size = sizeof(chat_msgs);



    server_start(serv);
    server_run(serv);
    return 0;
}