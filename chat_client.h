#include "chat.h"
#include <netdb.h>
#include <string.h>
// #include <sys/socket.h>

void usage(void)
{
    printf("chat_client addr port\n");
}

int build_client(char *host, char *port)
{
    struct addrinfo hints, *result, *rp;
    int ecode, sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((ecode = getaddrinfo(host, port, &hints, &result))) {
        error("client getaddrinfo\n");
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0)
            continue;

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(sockfd);
    }
    freeaddrinfo(result);
    if (rp == NULL)
        error("could not connect\n");

    return sockfd;
}

int main(int argc, char *argv[])
{
    char *host;
    char *port;
    int sockfd;
    int nread;
    char buf[BUFSIZE];
    // char msg_buf[BUFSIZE + sizeof(struct message)];
    // char msg_header[sizeof()]
    struct message msg;

    if (argc < 3) {
        usage();
        exit(EXIT_FAILURE);
    }
    char *host = argv[1];
    char *port = argv[2];

    memset(&msg, 0, sizeof(msg));
    msg.signature = MESSAGE_SIGNATURE;
    msg.version = 1.0;

    sockfd = build_client(host, port);
    while((nread = read(STDIN_FILENO, buf, BUFSIZE)) > 0) {
        msg.length = nread;
        msg.body = buf;
        /* rio_write */

    }
    return 0;
}