#pragma once
// acceptSocket相当于fd
// acceptChannel封装了fd
// newConnectionCallback将新链接交给其他loop处理

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"
#include <functional>
#include "InetAddress.h"

class EventLoop;

//主loop的
class Acceptor : noncopyable
{
public:

    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();
    EventLoop *loop_; // acceptor用的就是用户定义的那个baseloop，即mainloop
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};