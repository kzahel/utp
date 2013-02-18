#ifndef __UTPSOCKET_H__
#define __UTPSOCKET_H__

namespace UTP {
typedef unsigned char byte;

class Socket
{
public:
    enum { MAXBUFFER = 65536 };

    Socket(int af);
    ~Socket();
    int listen();
    int connect(struct sockaddr *sa, socklen_t salen);
    int recv(char *buf, size_t len);
    int send(const char *buf, size_t len);
    Socket *accept(struct sockaddr *sa, socklen_t *salen);
    int get_sock() { return udp_sock; }
    bool writable() { return iswritable; }
    bool closed() { return isclosed; }
    void handle_readable();

    static void check_timeouts();

protected:
    virtual void writable(bool iswritable) { this->iswritable = iswritable; }
    virtual void error(int errorcode) { }
    virtual void overhead(bool send, size_t count, int type) { }

private:
    int bindsocket();

    static void _SendToProc(void *userdata, const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen);
    static void _UTPGotIncomingConnection(void *userdata, struct UTPSocket *s);
    static void _UTPOnReadProc(void *userdata, const byte *bytes, size_t count);
    static void _UTPOnWriteProc(void *userdata, byte *bytes, size_t count);
    static size_t _UTPGetRBSize(void *userdata);
    static void _UTPOnStateChange(void *userdata, int state);
    static void _UTPOnError(void *userdata, int errcode);
    static void _UTPOnOverhead(void *userdata, bool send, size_t count, int type);

    Socket(struct UTPSocket *s);

    int address_family;
    bool bound, iswritable, isclosed;
    char *inbuffer, *outbuffer;
    size_t insize, outsize;
    int udp_sock;
    struct UTPSocket *utp_sock;
    Socket *next;
    static struct UTPFunctionTable utp_funcs;
};
}

#endif/*__UTPSOCKET_H__*/
