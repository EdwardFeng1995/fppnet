#ifndef FPPNET_SRC_NET_POLLER_H
#define FPPNET_SRC_NET_POLLER_H

#include <map>
#include <vector>

#include "../base/datetime/Timestamp.h"
#include "EventLoop.h"

struct pollfd;

namespace fppnet
{

class Channel;

//IO Multiplexing with poll(2)
//
//This class doesn't own the Channel objets.
class Poller : boost::noncopyable
{
public:
    typedef std::vector<Channel*> ChannelList;

    Poller(EventLoop* loop);
    ~Poller();

    //核心函数，调用poll(2)获得当前活动的I/O事件，
    //填入activeChannels，并返回poll(2) return的时刻
    muduo::Timestamp poll(int timeoutMs, ChannelList* activeChannels);

    //更新Channel关心的事件， 更新和维护pollfds_数组
    void updateChannel(Channel* channel);

    // 移除channel，必须在loop线程调用
    void removeChannel(Channel* channel);

    //断言本Poller在创建loop的线程里
    void assertInLoopThread() {
        ownerLoop_->assertInLoopThread();
    }

private:
    //填充活跃Channel列表
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    
    using PollFdList = std::vector<struct pollfd>;
    using ChannelMap = std::map<int, Channel*>;

    EventLoop* ownerLoop_; //本Poller属于哪个loop_
    PollFdList pollfds_;   //监听的文件描述符数组
    ChannelMap channels_;  //Channel映射表
};

}
#endif
