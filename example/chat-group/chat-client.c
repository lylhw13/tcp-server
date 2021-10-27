#include "chat.h"
#include "generic.h"

#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <stddef.h>
#include <fcntl.h>

/*
 * the client has two mode: client mode or server mode
 */

static void usage(const char *program)
{
    fprintf(stderr,
        "client mode: %s addr port\n"
        "server mode: %s port\n", program, program);
    exit(EXIT_FAILURE);
}


int build_server(const char *port)
{
    int listenfd, connfd;
    struct sockaddr_storage cliaddr;
    socklen_t cliaddr_len;

    listenfd = create_and_bind(port);

    if (listen(listenfd, SOMAXCONN) < 0)
        error("listen");
    for (;;) {
        cliaddr_len= sizeof(cliaddr);

        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);
        if (connfd >= 0)
            break;
    }

    return connfd;
}

int main(int argc, char *argv[])
{
    char *host, *port;
    int sockfd = -1;

    if (argc == 2) {
        port = argv[1];
        sockfd = build_server(port);
    }
    if (argc == 3) {
        host = argv[1];
        port = argv[2];
        sockfd = create_and_connect(host, port);
    }

    if (sockfd < 0)
        usage(argv[0]);

    setnonblocking(sockfd);
    readwrite(sockfd);
    
    return 0;
}