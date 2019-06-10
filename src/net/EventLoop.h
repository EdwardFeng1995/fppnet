#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include "../base/datetime/Timestamp.h"
#include "../base/thread/Thread.h"
#include "Callbacks.h"
#include "TimerId.h"

#include <boost/scoped_ptr.hpp> 
#include <vector>
#include <mutex>

namespace muduo
{

class Channel;
class Poller;
class EPoller;
class TimerQueue;

class EventLoop :boost::noncopyable 
{
public:
    EventLoop();
    ~EventLoop();

    //接收用户任务回调
    using Functor = std::function<void()>;

    //EventLoop开始工作的函数
    void loop();
    //EventLoop结束工作的函数
    void quit();
    
    //poll返回的时间，意味着数据到达
    Timestamp pollReturnTime() const { return pollReturnTime_; }

    //在它的IO线程内执行某个用户任务回调
    void runInLoop(const Functor& cb);

    //将cb放入队列，并在必要时换线IO线程
    void queueInLoop(const Functor& cb);

    //唤醒IO线程
    void wakeup();

    //给用户新增的定时器接口
    //Runs callback at 'time'.
    //创建一个不能定期重复运行的定时器，在time时刻调用定时器回调，返回一个定时器指针
    TimerId runAt(const Timestamp& time, const TimerCallback& cb);
    
    //Runs callback after @c delay seconds.
    //创建一个不能重复定期运行的定时器，让其中的回调函数在距现在的delay时间后运行
    TimerId runAfter(double delay, const TimerCallback& cb);
    
    //Runs callback every @c interval seconds.
    //创建一个可以定期重复运行的定时器，让其中的回调函数从现在开始定期运行
    TimerId runEvery(double interval, const TimerCallback& cb);

    // 注销定时器
    void cancel(TimerId timerId);

    //当前线程不是创建对象时的线程，就报错
    void assertInLoopThread() {
        if(!isInLoopThread()) {
            abortNotInloopThread();
        }
    }
    
    //检查当前线程id和创建时记录的线程id是否相等
    bool isInLoopThread() const {
        return threadId_ == muduo::CurrentThread::tid();
    }

    //
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    
    //当前线程不是创建对象时的线程时，报错的信息，输出循环所在线程id和当前线程id
    void abortNotInloopThread();

    // 给wakeupChannel_传入作回调函数，读取经过Event::Wakeup()写入过数据的文件
    // Event::Wakeup()写入文件后会唤醒poller，这样就会检测到读事件，从而返回到Event::Loop()中
    // 由wakeup_fd_对应的wakeupChannel_执行handleRead()函数
    void handleRead();
    
    //执行由非当前IO线程传入的任务回调函数
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    bool looping_; /*atomic*/                   // 记录当前循环是否已启动

    bool quit_;

    bool callingPendingFunctors_; /*atomic*/    // 记录当前线程是不是在运行其他线程传给pendingFunctors_的回调函数

    const pid_t threadId_;                      // 线程id

    Timestamp pollReturnTime_;          

    //std::unique_ptr<Poller> poller_;            // 指向Poller，通过该指针来间接持有Poller
    std::unique_ptr<EPoller> epoller_;
    std::unique_ptr<TimerQueue> timerQueue_;    // 定时器队列

    int wakeupFd_;                              // 生成的用于唤醒当前阻塞poller_的文件描述符

    std::unique_ptr<Channel> wakeupChannel_;    // 负责wakeupFd_的channel，内部类，不暴露给客户
                                                // 用于处理wakeupFd_上的readable事件，将事件分发至handleRead()

    ChannelList activeChannels_;                // 活跃Channels列表

    std::mutex mutex_;                           // pendingFunctors_暴露给了其他线程，需要mutex保护

    std::vector<Functor> pendingFunctors_;      // 接收其他线程传入的回调函数，
                                                //经过RunInLoop--->QueueInLoop，然后储存在pendingFunctors_里

};

}

#endif
