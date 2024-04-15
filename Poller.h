#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include <vector>
#include <unordered_map>

class Channel; // 指针类型，声明就行
class EventLoop;
// muduo中多路事件分发起的核心IO复用模块
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一接口
    // 激活的channel，感兴趣的channel，需要poller照顾的channel
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channel是否在当前poller中
    bool hasChannel(Channel *channel) const;
    //为什么不把这个写在cc里呢， 这个是要生成具体io复用对象，那么要包含pollpoller.h和epollpoller.h，基类类不能引用派生类
    //muduo是建了一个新的类，用来专门做这个方法，里头是判断环境变量是否有用poll，否则都是用epoll
    // 获取这个事件循环默认poller 相当于instance
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    //key:sockfd, value:sockfd所属的channel
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_; // 定义poller所属的事件循环el
};