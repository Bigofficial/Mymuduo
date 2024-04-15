#include "EventLoop.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include "Logger.h"
#include <errno.h>
#include "Poller.h"
#include "Channel.h"
#include <memory>
// 防止一个线程创建多个EventLoop __thread相当于thread_local机制
__thread EventLoop *t_loopInThisThread = nullptr; //__thread 关键字用于声明了一个线程局部变量 t_loopInThisThread，其含义是每个线程都有一个独立的 EventLoop 指针，而且这个指针只在当前线程中有效，不同线程中的 t_loopInThisThread 变量互不影响。

// 定义默认poller IO复用接口超市时间
const int kPollTimeMs = 10000;

// 创建wakeupfd, 用来notify唤醒subreactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); // 创建新的事件文件描述符 nonblock表示非阻塞， cloexec表示执行exec系列函数时关闭文件描述符
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop() : looping_(false), quit_(false), callingPendingFunctors_(false), threadId_(CurrentThread::tid()), poller_(Poller::newDefaultPoller(this)), wakeupFd_(createEventfd()), wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)
    {
        // 已经有一个loop了
        LOG_FATAL("Anothre EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的事件类型，以及回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个eventloop豆浆监听wakeupchannel的EPOLLIN读事件了，main给sub通知
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll(); // 对所有事件都不感兴趣
    wakeupChannel_->remove();     // 从poller删除
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

//就是为了唤醒
void EventLoop::handleRead()
{
    uint64_t one = 1; // 发8字节
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd   一种是client的fd，一种wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // Poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程 mainLoop accept fd《=channel subloop
         * mainLoop 事先注册一个回调cb（需要subloop来执行）    wakeup subloop后，执行下面的方法，执行之前mainloop注册的cb操作
         */ 
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 退出事件循环， 1.loop在自己线程中调用quit，所以此时他不会被阻塞在poll中
// 2.非loop线程中，调用loop的quit
/*
    mainloop

    ==== 生产者-消费者的线程安全队列（这是没有的，我们用wakeupFd

    subloop1 subloop2 subloop3


*/
void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())
    {             // 如果是在其他线程中调用quit，eg在subloop（worker）中调用了mainloop（io）的quit mainloop可能在poll阻塞了
        wakeup(); // poll唤醒
    }
}

// 在当前loop种执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前loop线程中，执行cb
    {
        cb();
    }
    else
    { // 在非当前loop线程执行cb，就需要唤醒loop所在线程执行cb
        queueInLoop(cb);
    }
}
// 把cb放入队列，唤醒loop所在线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{   
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb); // 在底层内存构造一个cb， pushback时拷贝构造，这是直接构造
    }

    // 唤醒相应的，需要执行上面回调操作的loop线程
    // 1.不是当前执行的线程 2.我是在当前线程，但是我正在执行回调 上一轮的dopendingfunctors还没执行完，loop又加了新的functor，但是当它pendingfunctors执行完了，那么他就被阻塞在poll中，所以这里加上这个calling来去唤醒让他继续do
    if (!isInLoopThread() || callingPendingFunctors_) 
    {             // 这里的callingPendingFunctors_待解释
        wakeup(); // 唤醒loop所在线程
    }
}

// 唤醒loop所在线程 向wakeupfd写数据 wakeupChannel就发生读事件，线程唤醒
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("Eventloop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}

// eventlop的方法 =》调用poller的方法
void EventLoop::updateChannel(Channel *channel){
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel){
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel){
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors(){
    //用交换的话可以同时放，取
    std::vector<Functor> functors;
    callingPendingFunctors_ = true; //需要执行回调
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_); //不妨碍我还有没有执行完的回调，也不妨碍其他人继续往pendingfunctors放东西
    }
    
    for(const Functor &functor : functors){
        functor(); //执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}