#include "Channel.h"
#include <sys/epoll.h>
#include "EventLoop.h"
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd) : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

Channel::~Channel()
{
    
}

// channel的tie方法什么时候调用过 连接一个TcpConnection新链接创建的时候 TcpConnection  => channel
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

void Channel::update()
{
    // 通过channel所属的el，调用poller的相应方法，注册fd的events事件
      loop_->updateChannel(this);
}

// 在el中删除这个channel
void Channel::remove()
{
      loop_->removeChannel(this);
}

//
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock(); // 提升
        if (guard)
        {
            hanleEventWithGuard(receiveTime);
        }
    }
    else
    {
        hanleEventWithGuard(receiveTime);
    }
}
// 根据poller通知的channel发生的具体事件，由channel执行具体回调
void Channel::hanleEventWithGuard(Timestamp receiveTime)
{

    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}