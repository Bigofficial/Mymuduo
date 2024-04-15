#pragma once

#include "noncopyable.h"
class InetAddress;

class Socket : noncopyable
{
public:
    // 防止隐式产生对象
    explicit Socket(int sockfd) : sockfd_(sockfd)
    {
    }

    ~Socket();

    int fd() const { return sockfd_; }

    void bindAddress(const InetAddress &Localaddr);

    void listen();

    int accept(InetAddress *peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);//直接放松，不缓冲

    void setReuseAddr(bool on);

    void setReusePort(bool on);

    void setKeepAlive(bool on);

private:
    const int sockfd_;
};