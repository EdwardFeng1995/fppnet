/*
 * poller 的epoll版本，muduo实际的做法是定义Poller基类并提供两份实现PollPoller和EPollPoller
 */
#ifndef MUDUO_NET_EPOLLER_H
#define MUDUO_NET_EPOLLER_H

#include <boost/noncopyable.hpp>
#include <map>
#include <vector>

#include "../base/datetime/Timestamp.h"
#include "EventLoop.h"

struct epoll_event;

namespace muduo
{

class Channel;

// 用epoll(4)来IO Multiplexing
// 该类不含Channel对象
//
class EPoller : boost::noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    EPoller(EventLoop* loop);
    ~EPoller();

    // Polls the I/O events.
    // Must be called in the loop thread.
    Timestamp poll(int timeoutMs, ChannelList* activeChannels);

    /// Changes the interested I/O events.
    /// Must be called in the loop thread.
    void updateChannel(Channel* channel);
    /// Remove the channel, when it destructs.
    /// Must be called in the loop thread.
    void removeChannel(Channel* channel);

    void assertInLoopThread() { ownerLoop_->assertInLoopThread(); }

private:
    static const int kInitEventListSize = 16;

    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    void update(int operation, Channel* channel);

    using EventList = std::vector<struct epoll_event>;
    using ChannelMap = std::map<int, Channel*>;

    EventLoop* ownerLoop_;
    int epollfd_;
    EventList events_;
    ChannelMap channels_;
};

}
#endif
