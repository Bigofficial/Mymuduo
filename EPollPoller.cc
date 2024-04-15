#include "EPollPoller.h"
#include "Logger.h"
#include "errno.h"
#include <unistd.h>
#include "Channel.h"
#include <strings.h>
#include <string.h>

const int kNew = -1;    // channel还没被添加到poller channel成员index_ = -1
const int kAdded = 1;   // channel已经添加到poller
const int kDeleted = 2; // channel已经删除
EPollPoller::EPollPoller(EventLoop *loop) : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    //取得第一个元素地址 events.begin是迭代器
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels); //返还给eventloop发生的事件对应活跃的channel
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) // 填写活跃的channel
{

    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表
    }
    // revents发生事件
}


// epoll ctl
// channel update remove -> eventloop updateChannel removeChannel -> poller的
/*
        eventloop  => poller.poll
     channellist poller
                 channelmap <fd, channel*>
*/
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);
    if (index == kNew || index == kDeleted)
    {
        int fd = channel->fd();
        if (index == kNew)
        { // 从来没添加到map
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }

    else
    { // channel已经在poller上注册过了
        int fd = channel->fd();
        if (channel->isNoneEvent()) //
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}
void EPollPoller::removeChannel(Channel *channel)
{

    // 从poller的channelmap中删除
    int fd = channel->fd();
    channels_.erase(fd);
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);
    // 从epoll_event删除
    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }

    // 设置index
    channel->set_index(kNew);
}


// 更新channel通道 add/del/mod
// epoll_data这个数据结构里头有个void*ptr我们用来绑定channel
void EPollPoller::update(int operation, Channel *channel)
{
    //这个event是客户端的fd感兴趣的事件
    epoll_event event;
    bzero(&event, sizeof event);
    int fd = channel->fd();
    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;
    // 我要监听fd的哪些事件
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) //修改客户端fd感兴趣的事件
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}

