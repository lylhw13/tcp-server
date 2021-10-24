#include "generic.h"
#include "chat.h"

#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>

#define POLL_STDIN 0
#define POLL_NETOUT 1
#define POLL_NETIN 2
#define POLL_STDOUT 3

// static void error(const char *str)
// {
//     perror(str);
//     exit(EXIT_FAILURE);
// }

void readwrite(int sockfd)
{
    struct pollfd pfds[4];
    int numfds;
    int timeout = 0;
    unsigned char stdinbuf[BUFSIZE];
    unsigned char netinbuf[BUFSIZE];
    int stdinbufpos = 0;

    int netinbufpos = 0;
    int netinparsepos = 0;
    int netinwritepos = 0;

    int i, nread, nwrite, length, nwriteall;
    struct message msg;
    struct message *msg_ptr;
    msg.signature = MESSAGE_SIGNATURE;
    int offset = offsetof(struct message, body);
    memcpy(stdinbuf, &msg, offset);

    pfds[POLL_STDIN].fd = STDIN_FILENO;
    pfds[POLL_STDIN].events = POLLIN;

    pfds[POLL_NETOUT].fd = sockfd;
    pfds[POLL_NETOUT].events = 0;

    pfds[POLL_NETIN].fd = sockfd;
    pfds[POLL_NETIN].events = POLLIN;

    pfds[POLL_STDOUT].fd = STDOUT_FILENO;
    pfds[POLL_STDOUT].events = 0;

    while (1)
    {
        /* no read */
        if (pfds[POLL_STDIN].fd == -1 && pfds[POLL_NETIN].fd == -1) 
                return;

        /* no write */
        if (pfds[POLL_STDOUT].fd == -1 && pfds[POLL_NETOUT].fd == -1)
            return;

        numfds = poll(pfds, 4, timeout);

        if (numfds < 0)
            error("poll");

        if (!numfds)
            continue;

        /* check fd conditions */
        for (i = 0; i < 4; ++i)
            if (pfds[i].revents & (POLLERR | POLLNVAL))
                pfds[i].fd = -1;


        /* stdin fo stdinbuf */
        if (pfds[POLL_STDIN].revents & POLLIN && stdinbufpos < BUFSIZE)
        {
            if (stdinbufpos < offset)
                stdinbufpos = offset;

            errno = 0;
            nread = read(pfds[POLL_STDIN].fd, stdinbuf + stdinbufpos, BUFSIZE - stdinbufpos);
            if (nread == 0) {   /* end of file */
                pfds[POLL_STDIN].fd = -1;
                continue;
            }
            if (nread < 0) {
                if (errno == EAGAIN)
                    continue;
                else 
                    error("stdin read");

            }
            msg_ptr = (struct message*)stdinbuf;
            msg_ptr->length = nread;
            stdinbufpos += nread;
            
            if (nread > 0) {
                pfds[POLL_NETOUT].events = POLLOUT;
                pfds[POLL_STDIN].events = 0;
            }
        }

        /* netout from stdinbuf */
        if (pfds[POLL_NETOUT].revents & POLLOUT && stdinbufpos > 0)
        {
            errno = 0;
            nwrite = write(pfds[POLL_NETOUT].fd, stdinbuf, stdinbufpos);
            if (nwrite < 0) {
                if (errno == EAGAIN)
                    continue;
                else 
                    error("netout write");

            }

            stdinbufpos -= nwrite;
            memmove(stdinbuf, stdinbuf + nwrite, stdinbufpos);

            if (stdinbufpos == 0) {
                pfds[POLL_NETOUT].events = 0;
                pfds[POLL_STDIN].events = POLLIN;
            }
        }

        /* netin to netinbuf */
        if (pfds[POLL_NETIN].revents & POLLIN && netinbufpos < BUFSIZE)
        {
            errno = 0;
            nread = read(pfds[POLL_NETIN].fd, netinbuf + netinbufpos, BUFSIZE - netinbufpos);
            
            if (nread == 0) {   /* connection close */
                pfds[POLL_NETIN].fd = -1;
                continue;
            }
            if (nread < 0) {
                if (errno == EAGAIN)
                    continue;
                else 
                    error("netin read");

            }

            netinbufpos += nread;

            if (netinbufpos > 0)
                pfds[POLL_STDOUT].events = POLLOUT;

            if (netinbufpos == BUFSIZE)
                pfds[POLL_NETIN].events = 0;
        }

        /* stdout from netinbuf */
        if (pfds[POLL_STDOUT].revents & POLLOUT && netinbufpos > 0)
        {
            // netinwritepos = netinparsepos;
            while (1) {
                if (netinwritepos == netinparsepos) {

                    if (netinbufpos - netinparsepos < offset)
                        break;

                    msg_ptr = (struct message *)(netinbuf + netinparsepos);
                    if (msg_ptr->signature != MESSAGE_SIGNATURE) 
                        error("error message signature\n");
                    
                    length = msg_size(msg_ptr);

                    if (length > BUFSIZE)
                        error("too large message\n");
                    
                    if (length > netinbufpos)
                        break;
                    netinwritepos = netinparsepos + offsetof(struct message, body);
                    netinparsepos += length;
                }
                
                errno = 0;
                nwrite = write(pfds[POLL_STDOUT].fd, netinbuf + netinwritepos, netinparsepos - netinwritepos);
                if (nwrite < 0) {
                    if (errno == EAGAIN)
                        continue;
                    else 
                        error("stdout write");
                }
                netinwritepos += nwrite;
            }

            memmove(netinbuf, netinbuf + netinparsepos, netinbufpos - netinparsepos);
            netinbufpos -= netinparsepos;
            netinparsepos = 0;
            netinwritepos = 0;

            if (netinparsepos == 0) {
                pfds[POLL_STDOUT].events = 0;
                pfds[POLL_NETIN].events = POLLIN;
            }
        }
    }

}