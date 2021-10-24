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

void print_msg(struct message_queue*head)
{
    LOGD("%s\n", __FUNCTION__);
    struct message_entry *msg_entry = STAILQ_FIRST(head);
    while(msg_entry != NULL) {
        printf("%.*s\n",msg_entry->ptr->length, msg_entry->ptr->body);
        msg_entry = STAILQ_NEXT(msg_entry, entries);
    }
    LOGD("end %s\n", __FUNCTION__);
    return;
}

int on_read_message_complete(tcp_session_t *session)
{
    LOGD("%s\n", __FUNCTION__);
    int err, length, offset;
    struct message *msg_begin;
    chat_messages_queue_t *msg_info;

    msg_begin = (struct message *)session->read_buf;
    msg_info = (chat_messages_queue_t*)session->additional_info;
    offset = offsetof(struct message, body);

    while (1) {
        if ((session->read_pos - session->parse_pos) < offset)
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
        LOGD("message size %d, read size %d\n", length, (int)(session->read_pos - session->parse_pos));
        if (length > (session->read_pos - session->parse_pos))
            return MESSAGE_PARTIAL;

        struct message *ptr = (struct message *)malloc(length);
        if (ptr == NULL)
            error("malloc message\n");
        memcpy(ptr, session->read_buf + session->parse_pos, length);
        session->parse_pos += length;

        struct message_entry *msg_entry = (struct message_entry *)malloc(sizeof(struct message_entry));
        if (msg_entry == NULL)
            error("malloc message_entry\n");
        msg_entry->ptr = ptr;
        
        // LOGD("before lock\n");
        err = pthread_mutex_trylock(msg_info->lock);
        if (err == EBUSY)
            return MESSAGE_LOCK_AGAIN;
        if (err != 0)
            return MESSAGE_ERROR;
        STAILQ_INSERT_TAIL(msg_info->message_queue_head, msg_entry, entries);
        (*(msg_info->msg_total_num))++;
        printf("after msg num is %d\n", *(msg_info->msg_total_num));
        pthread_mutex_unlock(msg_info->lock);

        print_msg(msg_info->message_queue_head);
        /* shift buffer */
        memmove(session->read_buf, session->read_buf + session->parse_pos, session->read_pos - session->parse_pos);
        session->read_pos -= session->parse_pos;
        session->parse_pos = 0;

        // return MESSAGE_OK;
    }

    LOGD("end %s\n", __FUNCTION__);

    return MESSAGE_PARTIAL;
}

int on_write_message_complete(tcp_session_t *session)
{
    // LOGD("%s\n", __FUNCTION__);
    int err;
    struct message *msg;
    struct message_entry *msg_entry;
    struct chat_messages_queue *msg_info;

    /* last write has not complete */
    if (session->write_pos <= session->write_buf + session->write_size && session->write_size != 0)
        return WCB_AGAIN;

    // LOGD("begin to write\n");
    /* reset the write buffer */
    session->write_size = 0;
    session->write_pos = session->write_buf;


    msg_info = (chat_messages_queue_t*)session->additional_info;
    err = pthread_mutex_trylock(msg_info->lock);
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
    printf("read to write %.*s\n",(int)session->write_size, session->write_pos);
    pthread_mutex_unlock(msg_info->lock);

    // LOGD("end %s\n", __FUNCTION__);

    return WCB_OK;
}



int main(int argc, char *argv[])
{
    server_t *serv;
    char *host = "127.0.0.1";
    char *port = "33333";
    int conn_loop_num = 2;
    chat_messages_queue_t chat_msgs;

    pthread_mutex_t lock;
    int msg_total_num;
    struct message_queue message_queue_head;

    pthread_mutex_init(&lock, NULL);
    msg_total_num = 0;
    STAILQ_INIT(&message_queue_head);   /* message queue for group */


    serv = server_init(host, port, conn_loop_num);

    serv->read_complete_cb = on_read_message_complete;
    serv->write_complete_cb = on_write_message_complete;

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