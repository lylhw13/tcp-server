#include "chat.h"
#include <netdb.h>
#include <string.h>
// #include <stddef.h>
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

void rio_write(int fd, char *buf, size_t len)
{
    int n, nwrite = 0;
    while (len > 0) {
        n = write(fd, buf + nwrite, len);
        if (n < 0)
            error("write");
        nwrite += n;
        len -= n;
    }
}

int main(int argc, char *argv[])
{
    char *host, *port;
    int sockfd;
    int nread;
    char buf[BUFSIZE];
    struct message msg;
    int body_pos;


    if (argc < 3) {
        usage();
        exit(EXIT_FAILURE);
    }
    host = argv[1];
    port = argv[2];

    memset(&msg, 0, sizeof(msg));
    body_pos = offsetof(struct message, body);
    msg.signature = MESSAGE_SIGNATURE;
    msg.version = 1.0;

    sockfd = build_client(host, port);
    
    while((nread = read(STDIN_FILENO, buf + body_pos, BUFSIZE - body_pos)) > 0) {
        msg.length = nread;
        memcpy(buf, &msg, body_pos);
        rio_write(sockfd, buf, nread + body_pos);
    }
    return 0;
}