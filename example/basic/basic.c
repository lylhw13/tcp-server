#include "../../generic.h"

int main()
{
    server_t *serv;
    char *host = "127.0.0.1";
    char *port = "33333";
    int conn_loop_num = 2;
    serv = server_init(host, port, conn_loop_num);

    server_start(serv);
    server_run(serv);
    
    return 0;
}