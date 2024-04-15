#pragma once
#include "Poller.h"
#include <vector>
#include <sys/epoll.h>
#include "Timestamp.h"

class Channel;
/*
    epoll使用
    epoll_create
    epoll_ctl add/mode/del 设置合适的事件
    epoll_wait
*/
class EPollPoller : public Poller
{

public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    //重写基类重写方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    //
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;
    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels); // 填写活跃的channel  

    //更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;
    
    // epoll相关都要用到的fd要通过epollfd来创建映射epoll底层产生的特殊
    int epollfd_;
    // epollwait返回发生的事件fd
    EventList events_;
};