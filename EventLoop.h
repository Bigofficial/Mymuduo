#pragma once
#include "noncopyable.h"
#include <functional>
#include <vector>
#include <atomic>
#include "Timestamp.h"
#include <memory>
#include <vector> // demultiplex epoll
#include <mutex>
#include "CurrentThread.h"

class Channel;
class Poller;
//question ： 为什么这里dopendingfactor不要包括read的吗，用void
// reactor eventloop
// 事件循环类 主要包含了两大模块 Channel Poller（epoll的抽象）他还实现了poll
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime() const {return pollReturnTime_;}

    //一个是在当前loop执行，一个是放到队列中执行
    //在当前loop种执行cb
    void runInLoop(Functor cb);
    //把cb放入队列，唤醒loop所在线程，执行cb
    void queueInLoop(Functor cb);

    //唤醒loop所在线程
    void wakeup();
    //eventlop的方法 =》调用poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    //判断eventloop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid();}
private:

    void handleRead(); //wakeup
    void doPendingFunctors(); //执行回调逻辑

    using ChannelList = std::vector<Channel *>;
    //这两个是否继续循环
    std::atomic_bool looping_; // 原子操作bool，通过cas实现
    std::atomic_bool quit_;    // 标志推出loop循环

    const pid_t threadId_; // 记录当前loop所在线程id 工作线程都监听一组channel，每一组channel发生的事件都得在自己的el中处理

    Timestamp pollReturnTime_; // poller返回发生事件的channels时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_; // eventfd 主要作用：当mainloop获取一个新用户channel，通过轮训算法选择一个subloop，通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_; // 把wakeupfd封装起来

    ChannelList activeChannels_;
    //Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_; // 当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_; //存储loop需要执行的所有回调操作
    std::mutex mutex_; //互斥锁，用来保护上面vecotr容器线程安全操作


};