#include "generic.h"
#include <string.h>

int on_write_message_complete(tcp_session_t *session)
{
    if (session->write_buf == NULL) {
        session->write_buf = session->read_buf;
        session->write_pos = 0;
        session->write_size = session->read_pos;
        return WCB_AGAIN;
    }

    int length = session->read_pos - session->write_pos;
    memmove(session->write_buf, session->write_buf + session->write_pos, length);
    session->read_pos -= session->write_pos;
    session->write_size = session->read_pos;
    session->write_pos = 0;

    return WCB_OK;
}


int main()
{
    server_t *serv;
    char *host = "127.0.0.1";
    char *port = "33333";
    int conn_loop_num = 2;
    serv = server_init(host, port, conn_loop_num);

    serv->write_complete_cb = on_write_message_complete;

    server_start(serv);
    server_run(serv);
    
    return 0;
}