#include <unistd.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include "utpsocket.h"

int main(int argc, char **argv) {
    struct pollfd pfd[2];
    UTP::Socket sock(AF_INET), *accepted = NULL;

    if (argc == 1) {
        if (sock.listen() == 0) {
            struct sockaddr_in sa;
            socklen_t salen = sizeof(sa);
            getsockname(sock.get_sock(), (struct sockaddr *)&sa, &salen);
            printf("%s:%d\n", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
        } else {
            perror("failed to listen");
            return 1;
        }
    } else if (argc == 3) {
        struct sockaddr_in sa = { };
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = inet_addr(argv[1]);
        sa.sin_port = htons(atoi(argv[2]));
        if (sock.connect((struct sockaddr *)&sa, sizeof(sa)) == -1) {
            perror("failed to connect");
            return 1;
        }
    }

    while (true) {
        int res;
        char buf[1000];

        pfd[0].fd = 0;
        pfd[0].events = ((accepted ? accepted : &sock)->writable() ? POLLIN : 0) | POLLHUP | POLLERR;
        pfd[1].fd = (accepted ? accepted : &sock)->get_sock();
        pfd[1].events = POLLIN;

        UTP::Socket::check_timeouts();

        int npfd = poll(pfd, 2, 100);

        if (pfd[0].revents & POLLIN) {
            res = read(0, buf, sizeof(buf));
            if (res > 0)
                (accepted ? accepted : &sock)->send(buf, res);
            else
                return 0;
        }
        if (pfd[1].revents & POLLIN)
            sock.handle_readable();

        if (accepted && accepted->closed()) {
            delete accepted;
            accepted = NULL;
        }

        struct sockaddr_in sa;
        socklen_t salen = sizeof(sa);
        UTP::Socket *newsock = sock.accept((struct sockaddr *)&sa, &salen);
        if (newsock) {
            if (accepted)
                delete accepted;
            accepted = newsock;
            printf("accepted %s:%d\n", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
        }

        int recvd;
        while ((recvd = (accepted ? accepted : &sock)->recv(buf, sizeof(buf))) > 0) {
            write(1, buf, recvd);
        }
    }
}
