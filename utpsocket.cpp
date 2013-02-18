#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include "utp.h"
#include "utpsocket.h"

namespace UTP {

struct UTPFunctionTable Socket::utp_funcs {
    Socket::_UTPOnReadProc,
    Socket::_UTPOnWriteProc,
    Socket::_UTPGetRBSize,
    Socket::_UTPOnStateChange,
    Socket::_UTPOnError,
    Socket::_UTPOnOverhead
};

Socket::Socket(int af) : 
    address_family(af),
    bound(false), 
    iswritable(false),
    isclosed(false),
    udp_sock(-1),
    utp_sock(NULL),
    next(NULL),
    inbuffer(NULL),
    outbuffer(NULL),
    insize(0),
    outsize(0)
{
}

Socket::Socket(struct UTPSocket *s)
{
    utp_sock = s;
}

Socket::~Socket()
{
    if (bound && udp_sock != -1)
        close(udp_sock);
    if (utp_sock)
        UTP_Close(utp_sock);
}

int Socket::listen()
{
    if (bindsocket() == -1)
        return -1;
}

int Socket::connect(struct sockaddr *sa, socklen_t salen)
{
    if (bindsocket() == -1)
        return -1;
    if (!utp_sock) {
        utp_sock = UTP_Create(_SendToProc, this, sa, salen);
        UTP_SetCallbacks(utp_sock, &utp_funcs, this);
    }
    UTP_Connect(utp_sock);
}

int Socket::bindsocket()
{
    if (!bound) {
        struct sockaddr sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_family = address_family;
        if (udp_sock == -1)
            udp_sock = socket(address_family, SOCK_DGRAM, 0);
        if (bind(udp_sock, &sa, sizeof(sa)) == -1) {
            close(udp_sock);
            return -1;
        }
        int nonblock = 1;
        ioctl(udp_sock, FIONBIO, &nonblock);
        bound = true;
    }
    return 0;
}

void Socket::check_timeouts()
{
    UTP_CheckTimeouts();
}

void Socket::_SendToProc(void *userdata, const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen)
{
    Socket *self = (Socket*)userdata;
    sendto(self->udp_sock, p, len, 0, to, tolen);
}

void Socket::_UTPGotIncomingConnection(void *userdata, struct UTPSocket *s)
{
    Socket *self = (Socket*)userdata;
    Socket *nextSocket = new Socket(s);
    nextSocket->next = self->next;
    self->next = nextSocket;
    nextSocket->address_family = self->address_family;
    nextSocket->udp_sock = self->udp_sock;
    UTP_SetCallbacks(nextSocket->utp_sock, &nextSocket->utp_funcs, nextSocket);
}

int Socket::recv(char *buf, size_t len)
{
    if (udp_sock == -1 || utp_sock == NULL) return -1;
    if (insize > len) {
        memcpy(buf, inbuffer, len);
        memmove(buf, buf + len, insize - len);
        insize -= len;
        return len;
    } else {
        memcpy(buf, inbuffer, insize);
        free(inbuffer);
        len = insize;
        insize = 0;
        inbuffer = NULL;
        return len;
    }
}

int Socket::send(const char *buf, size_t len)
{
    if (len + outsize > MAXBUFFER) 
        len = MAXBUFFER - outsize;
    if (!len) return 0;
    char *newbuf = (char *)realloc((void *)outbuffer, len + outsize);
    if (newbuf == NULL) return -1;
    memcpy(newbuf + outsize, buf, len);
    outbuffer = newbuf;
    outsize += len;
    writable(UTP_Write(utp_sock, len));
    return len;
}

void Socket::handle_readable()
{
    char buf[2048];
    struct sockaddr sa;
    do {
        socklen_t salen = sizeof(sa);
        int res = recvfrom(udp_sock, buf, sizeof(buf), 0, &sa, &salen);
        if (res == -1) return;
        UTP_IsIncomingUTP(_UTPGotIncomingConnection, _SendToProc, this, (const byte *)buf, res, (const struct sockaddr *)&sa, salen);
    } while (true);
}

Socket *Socket::accept(struct sockaddr *sa, socklen_t *salen)
{
    Socket *newsock = next;
    if (newsock) {
        next = newsock->next;
        newsock->next = NULL;
        UTP_GetPeerName(newsock->utp_sock, sa, salen);
    }
    return newsock;
}

void Socket::_UTPOnReadProc(void *userdata, const byte *bytes, size_t count)
{
    Socket *self = (Socket *)userdata;
    char *newbuf = (char *)realloc((void *)self->inbuffer, count + self->insize);
    if (newbuf) {
        memcpy(newbuf + self->insize, bytes, count);
        self->insize += count;
        self->inbuffer = newbuf;
    }
}

void Socket::_UTPOnWriteProc(void *userdata, byte *bytes, size_t count)
{
    Socket *self = (Socket *)userdata;
    memcpy(bytes, self->outbuffer, count);
    if (self->outsize - count) {
        memmove(self->outbuffer, self->outbuffer + count, self->outsize - count);
        self->outsize -= count;
    } else {
        free(self->outbuffer);
        self->outbuffer = NULL;
        self->outsize = 0;
    }
}

size_t Socket::_UTPGetRBSize(void *userdata)
{
    Socket *self = (Socket *)userdata;
    return self->insize;
}

void Socket::_UTPOnStateChange(void *userdata, int state)
{
    Socket *self = (Socket *)userdata;
    self->iswritable =
        ((state == UTP_STATE_CONNECT) || 
         (state == UTP_STATE_WRITABLE));
    self->isclosed = (state == UTP_STATE_EOF);
}

void Socket::_UTPOnError(void *userdata, int errorcode)
{
    Socket *self = (Socket *)userdata;
    self->error(errorcode);
}

void Socket::_UTPOnOverhead(void *userdata, bool send, size_t count, int type)
{
    // XXX investigate call after delete
    // Socket *self = (Socket *)userdata;
    // self->overhead(send, count, type);
}

}
