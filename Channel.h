#pragma once
#include "noncopyable.h"
#include <functional>
#include "Timestamp.h"
#include "Logger.h"
#include <memory>
class EventLoop; // 类型前置声明 ,对外暴露头文件信息就更少

/*

    理清楚  EventLoop、Channel、Poller之间的关系   《= Reactor模型上对应 Demultiplex
    
    Channel理解为通道，封装了sockfd和其感兴趣的event如EPOLLIN，EPOLLOUT事件
    还绑定了poller返回的具体事件，这些事件有poller监听，事件发生后，channel调用对应方法进行处理
*/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd); // 这里用指针，不管什么类型指针大小都是4字节，但是下面的Timestamp就不行他是变量，要知道其大小
    ~Channel();

    // fd得到poller通知以后，处理事件的，调用相应方法
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); } // cd要转出右值，出了函数局部对象就不需要了

    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }

    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }

    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止channel被手动remove掉，channel还在执行上面的回调操作
    void tie(const std::shared_ptr<void> &);

    int fd() const { return fd_; }

    int events() const { return events_; }

    // channel无法监听自己发生的事件，而是有epoll监听，监听之后要设置fd表达的事件，所以要提供这个接口
    int set_revents(int revt) { return revents_ = revt; }


    // 设置fd相应事件状态
    // fd对读事件感兴趣
    // 设置fd相应的事件状态
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread  eventloop包含channel，这个表示当前channel属于哪个el
    // 一个线程有一个el，一个el有一个poller，一个poller监听多个channel
    EventLoop *ownerLoop() { return loop_; }
    // 删除channel
    void remove();

    // eventHandling 用来记录析构的时候没有回调函数，每次处理完事件也是false
    //
private:
    //  当改变channel所表示fd的events事件后，update负责在poller里吗更改fd相应的事件epoll_ctl;
    //他两都属于 el
    void update();
    void hanleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd, poller监听的对象   epoll_ctl
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // poller返回的具体发生的事件
    int index_;       //表示channel状态 knew ，kadded， kdeleted

    // 语义观察者，观察一个强指针
    std::weak_ptr<void> tie_;

    bool tied_;

    // 四个函数对象，绑定外部传过来的操作
    // 因为channel通道里边能够获知fd最终发生的具体事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback  readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};


/*
EPOLLIN: 表示关联的文件描述符可以进行读取操作，即文件描述符可读。
EPOLLOUT: 表示关联的文件描述符可以进行写入操作，即文件描述符可写。
EPOLLPRI: 表示有紧急数据可读，这个标志通常用于带外数据的处理。
EPOLLERR: 表示关联的文件描述符发生了错误。
EPOLLHUP: 表示关联的文件描述符被挂起，可能是对端关闭了连接，或者出现了某些错误情况。
EPOLLET: 表示将 EPOLL 设置为边缘触发模式（Edge Triggered），在这种模式下，只有当文件描述符状态发生变化时才会触发事件通知。
EPOLLONESHOT: 表示将 EPOLL 设置为一次性模式（One-Shot），在这种模式下，每次事件发生后都需要重新添加文件描述符到 EPOLL 集合中才能继续监视其状态变化。
*/