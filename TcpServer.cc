#include "TcpServer.h"
#include "Logger.h"
#include <functional>
#include <strings.h>
#include "TcpConnection.h"
EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &nameArg,
                     Option option) : loop_(loop), ipPort_(listenAddr.toIpPort()), name_(nameArg), acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)), threadPool_(new EventLoopThreadPool(loop, name_)), connectionCallback_(), messageCallback_(), nextConnId_(1), started_(0)
{
    // 当有新用户连接时，执行newConnection
    // 这个函数做：根据轮训算法选择一个subloop，唤醒sl，把当前connfd封装成channel分发给subloop
    //  传用户和客户端通信的sockfd，还有客户端的IP地址端口号
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second); // 这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的tcpConnection资源
        item.second.reset();
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}
// 有一个新的客户端的连接，会执行这个 ，acceptor来传递
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    EventLoop *ioLoop = threadPool_->getNextLoop(); // 轮训选择一个subLoop，来管理loop
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_; // 不需要用atomic 就一个线程处理
    std::string connName = name_ + buf;

    LOG_INFO("TCPServer::NewConnection [%s] - new connection [%s] from %s\n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机地址和IP端口信息

    sockaddr_in local;

    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }

    InetAddress localAddr(local);
    // 根据连接成功的sockfd，创建Tcpconnection
    TcpConnectionPtr conn(new TcpConnection(
        ioLoop,
        connName,
        sockfd, // socket channel
        localAddr,
        peerAddr));
    connections_[connName] = conn;

    // 下面的回调都是用户设置给TcpServer=》TcpConnection=》channel=》Poller=》notify channel回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接调用 connectEstablished
    //为什么runinloop而不直接queueinloop呢，因为假如就一个loop的话就只能直接执行了
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
    
}
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
             name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}

// 设置底层subloop个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start()
{
    if (started_++ == 0) // 防止一个tcpserver对象被start多次
    {
        threadPool_->start(threadInitCallback_); // 启动底层loop线程池
        // 这个loop是主loop listen 然后把channel注册到这个poller上
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}