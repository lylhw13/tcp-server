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

void usage(void)
{
    fprintf(stderr,
        "clien state:  chat_client addr port\n"
        "server state: chat_client port\n");
    exit(EXIT_FAILURE);
}


int build_server(const char *port)
{
    int listenfd, connfd;
    struct sockaddr_storage cliaddr;
    socklen_t cliaddr_len;

    listenfd = create_and_bind(port);

    if (listen(listenfd, 64) < 0)
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
        sockfd = build_client(host, port);
    }

    if (sockfd < 0)
        usage();

    setnonblocking(sockfd);

    readwrite(sockfd);
    
    return 0;
}