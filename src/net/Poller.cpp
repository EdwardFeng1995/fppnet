#include "Poller.h"

#include "Channel.h"
#include "../base/logging/Logging.h"

#include <assert.h>
#include <poll.h>

using namespace fppnet;

Poller::Poller(EventLoop* loop) :
    ownerLoop_(loop)
{}

Poller::~Poller()
{}

//Poller的核心，调用poll(2)获得当前活动的I/O事件，
//填入activeChannel，并返回poll(2) return的时刻
muduo::Timestamp Poller::poll(int timeoutMs, ChannelList* activeChannels)
{
    //获取活跃Channels的数量
    int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);

    //获取当前时间
    muduo::Timestamp now(muduo::Timestamp::now());
    
    if(numEvents > 0) {
        LOG_TRACE << numEvents << " evnets happened";
        fillActiveChannels(numEvents, activeChannels);
    } else if (numEvents == 0) {
        LOG_TRACE << " nothing happened";
    } else {
        LOG_SYSERR << "Poller::poll()";
    }

    return now;
}

//填充活跃Channel列表
//遍历pollfd_，找出有活动事件的fd，把它对应的Channel填入activeChannels
void Poller::fillActiveChannels(int numEvnets, ChannelList* activeChannels) const
{
    for(PollFdList::const_iterator pfd = pollfds_.begin(); 
        pfd != pollfds_.end() && numEvnets > 0; ++pfd) {
        if (pfd->revents > 0) {
            --numEvnets;   //为了提前结束循环，每找到一个fd就递减
            ChannelMap::const_iterator ch = channels_.find(pfd->fd);
            assert(ch != channels_.end());
            Channel* channel = ch->second;
            assert(channel->Getfd() == pfd->fd);
            channel->set_revents(pfd->revents);    //保存活跃事件，供Channel::handleEvent()使用
            //pfd->revents = 0;
            activeChannels->push_back(channel);
        }
    }
}

//更新Channel关心的事件，更新和维护pollfd_数组
void Poller::updateChannel(Channel* channel)
{
    assertInLoopThread();
    LOG_TRACE << "fd = " << channel->Getfd() << " events = " << channel->Getevents();
    if (channel->index() < 0) {
        //a new one, add to pollfds_
        assert(channels_.find(channel->Getfd()) == channels_.end());
        struct pollfd pfd;
        pfd.fd = channel->Getfd();
        pfd.events = static_cast<short>(channel->Getevents());
        pfd.revents = 0;
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size())-1;    //channel在pollfds_数组中的下标
        channel->set_index(idx);
        channels_[pfd.fd] = channel;   //映射
    } else {
        // update exiting one
        assert(channels_.find(channel->Getfd()) != channels_.end());
        assert(channels_[channel->Getfd()] == channel);
        int idx = channel->index();   //获得channel在pollfds_中的位置
        assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
        struct pollfd& pfd = pollfds_[idx];
        assert(pfd.fd == channel->Getfd() || pfd.fd == -channel->Getfd()-1);  //channel->isNoneEvent(), pfd.fd=-1
        pfd.events = static_cast<short>(channel->Getevents());
        pfd.revents = 0;
        if (channel->isNoneEvent()) {
            //ignore this pollfd
            pfd.fd = -channel->Getfd()-1;
        }
    }
}

void  Poller::removeChannel(Channel* channel)
{
    LOG_INFO << "Poller::removeChannel被调用";
    assertInLoopThread();
    LOG_TRACE << "fd = " << channel->Getfd();
    // 确保三连
    assert(channels_.find(channel->Getfd()) != channels_.end()); // 确保要移除的channel在Channel map中
    assert(channels_[channel->Getfd()] == channel);              // 确保要移除的channel  fd映射是对的  
    assert(channel->isNoneEvent());                           // 确保要移除的channel 没有事件
    
    int idx  = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    const struct pollfd& pfd = pollfds_[idx];
    (void)pfd;
    assert(pfd.fd == -channel->Getfd()-1 && pfd.events == channel->Getevents());
    // 执行删除
    size_t n = channels_.erase(channel->Getfd());

    assert(n == 1);
    (void)n;

    if (muduo::implicit_cast<size_t>(idx) == pollfds_.size()-1) {
        pollfds_.pop_back();
    }
    else
    {
        int channelAtEnd = pollfds_.back().fd;
        // 将找到的元素位置和最后的元素交换
        iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);
        if (channelAtEnd < 0)
            channelAtEnd = -channelAtEnd - 1;   // ???没看懂
        // channels_设置新的下一个元素索引
        channels_[channelAtEnd]->set_index(idx);
        // pop掉最后一个元素
        pollfds_.pop_back();
    }

}
