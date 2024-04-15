#pragma once
#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include <string>
#include <memory>
#include <atomic>
#include "Buffer.h"
#include "Timestamp.h"
class Channel;
class EventLoop;
class Socket;

/*
    Tcpserver -> Acceptor ==有一个新用户连接，通过accept函数拿到connfd，
    打包成tcpconnection， 设置回调， 注册channel， channel 放到poller上，事件来了就调用channel回调
*/

// 已经建立连接的一条客户端，保存一些连接相关数据，最终Loop还是监听channel，所以加上socket打包成channel扔给poller，poller监听到有事件发生就会回调channel上的callback
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                  const std::string &name,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);

    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string &buf);
    
    // 关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

private:
    enum StateE
    {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };
    void setState(StateE state) { state_ = state; }
    
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_; // 绝对不是baseloop了，tcpconnection都是给subloop处理
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    //acceptor类似， 他是在mainloop tcpconnection是在subloop 都需要吧底层sockfd封装
    std::unique_ptr<Socket> socket_;
    
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_; // 当前主机
    const InetAddress peerAddr_;  // 对端的

    // 用户设置Tcpserver的  tcpserver扔给 -> tcpconnection 扔给channel ， poller监听channel，channel有数据就调用
    ConnectionCallback connectionCallback_;       // 有新链接的回调
    MessageCallback messageCallback_;             // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    
    Buffer inputBuffer_; //接收数据缓冲区
    Buffer outputBuffer_; //发送数据缓冲区
};