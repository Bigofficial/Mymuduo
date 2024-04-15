#include "Acceptor.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "Logger.h"
#include "errno.h"
#include "unistd.h"
static int createNonblocking()
{
    // sock_Stream:tcp
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create error:%d", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop), acceptSocket_(createNonblocking()), acceptChannel_(loop, acceptSocket_.fd()), listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    // TcpServer.start() Acceptor.listen有新用户连接要执行一个回调（connfd打包成channel唤醒一个subloop，给subloop
    // baseloop 的channel监听到有读事件就要用这个函数
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    //从baseloop poller里边全部清除掉，不在向poller注册读写事件
    acceptChannel_.disableAll();
    
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listen
    acceptChannel_.enableReading();//启动监听 acceptChannel_=>poller
}
// listenfd有事件 有新用户连接
void Acceptor::handleRead()
{
    InetAddress peerAddr; // 客户端ip地址端口号
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        // 执行回调
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr);//轮训找到subloop，唤醒，分发当前新客户端的channel

        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept error:%d", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}