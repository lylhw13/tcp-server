/* 
 * This is a basic example of using tcp-server library.
 * The basic server will send back everthing it recevied.
 */

#include "generic.h"
#include <string.h>

void usage(const char *program)
{
    fprintf(stderr, 
        "usage: %s port thread-num\n"
        "      thread-num should greater than 0\n", program);
    exit(EXIT_FAILURE);
}

int on_write_complete(tcp_session_t *session)
{
    if (session->write_buf == NULL) {
        session->write_buf = session->read_buf;
        session->write_pos = 0;
        session->write_size = session->read_pos;
        return WCB_AGAIN;
    }

    /* shift buffer */
    int length = session->read_pos - session->write_pos;
    memmove(session->write_buf, session->write_buf + session->write_pos, length);
    session->read_pos -= session->write_pos;
    session->write_size = session->read_pos;
    session->write_pos = 0;

    return WCB_OK;
}

int main(int argc, char *argv[])
{
    server_t *serv;
    char *port;
    int conn_loop_num;

    if (argc < 3)
        usage(argv[0]);

    port = argv[1];
    conn_loop_num = atoi(argv[2]);
    
    if (conn_loop_num == 0)
        usage(argv[0]);

    serv = server_init(port, conn_loop_num);

    serv->write_complete_cb = on_write_complete;

    server_start(serv);
    server_run(serv);
    
    return 0;
}