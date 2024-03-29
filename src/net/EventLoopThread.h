#ifndef FPPNET_SRC_NET_EVENTLOOPTHREAD_H
#define FPPNET_SRC_NET_EVENTLOOPTHREAD_H

#include <thread>
#include <mutex>
#include <condition_variable>

#include <boost/noncopyable.hpp>

namespace fppnet
{
class EventLoop;

class EventLoopThread : boost::noncopyable
{
public:
    EventLoopThread();
    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

}

#endif
