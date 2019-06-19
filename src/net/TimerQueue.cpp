#define __STDC_LIMIT_MACROS
#include "TimerQueue.h"

#include "../base/logging/Logging.h"
#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <sys/timerfd.h>

namespace fppnet
{
namespace detail
{

//创建定时器文件描述符
int creatTimerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    if(timerfd < 0) {
        LOG_FATAL << "Failed in timerfd_create";
    }

    return timerfd;
}

//计算时间when与现在的时间差
struct timespec howMuchTimeFromNow(muduo::Timestamp when)
{
    //计算出时间差
    int64_t microseconds = when.microSecondsSinceEpoch() - 
        muduo::Timestamp::now().microSecondsSinceEpoch();

    //如果时间差太小的话，就按100微妙算
    if(microseconds < 100)
        microseconds = 100;

    //将事件差转换成秒和微秒的单位
    struct timespec ts;
    ts.tv_sec = static_cast<time_t> (microseconds / muduo::Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long> ((microseconds % muduo::Timestamp::kMicroSecondsPerSecond) * 1000);

    return ts;
}

//读取定时器文件中的内容
void readTimerfd(int timerfd, muduo::Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    LOG_TRACE << "TimerQueue::handleRead()" << howmany << " at " << now.toString();
    if(n != sizeof howmany) {
        LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
    }
}

//重设定时器到期时间，这个函数在TimerQueue::AddTimer()中调用，就是在插入新的定时器以后，重设定时器队列的到期时间
void resetTimerfd(int timerfd, muduo::Timestamp expiration)
{
    // wake up loop by timerfd_settime()
    struct itimerspec newValue;
    struct itimerspec oldValue;
    bzero(&newValue, sizeof newValue);
    bzero(&oldValue, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret)
    {
        LOG_SYSERR << "timerfd_settime()";
    }
}

}
}



using namespace fppnet;
using namespace fppnet::detail;

TimerQueue::TimerQueue(EventLoop* loop) : 
    loop_(loop),
    timerfd_(creatTimerfd()),
    TimerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false)
{
    // 为定时器队列的channel设置读事件回调函数，并设置为可读事件，这样会把timerfd_注册进poller_的fd列表中
    TimerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
    TimerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    close(timerfd_);

    //如果用的不是智能指针，而是普通指针Timer* ,则需要手动回收
    for (TimerList::iterator it = timers_.begin();
         it != timers_.end(); ++it)
    {
        delete it->second;
    }
}

//经过修改，addtimer()拆分为两部分，拆分后只负责转发，真正的addTimerInLoop在IO线程工作
//该函数可以跨线程调用了，实现线程安全
TimerId TimerQueue::addTimer(const TimerCallback&cb, muduo::Timestamp when, double interval)
{
    LOG_DEBUG << "addTimer";
    //创建一个新的定时器
    Timer* timer = new Timer(cb, when, interval);
    //std::shared_ptr<Timer> timer = std::make_shared<Timer>(cb, when, interval);
    loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    LOG_DEBUG << "addTimerInLoop";
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);

    if (earliestChanged)
    {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

// 定时器注销程序
void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    if (it != activeTimers_.end()) {
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));   // 删除timers_中的timer
        assert(n == 1);
        (void)n;
        delete it->first;         // 释放内存timer的内存
        activeTimers_.erase(it);   // 删除avtiveTimers中的timer
    } else if (callingExpiredTimers_) {
        cancelingTimers_.insert(timer);
    }
    assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
    LOG_DEBUG << "handleRead";
    loop_->assertInLoopThread();
    //获取当前时间
    muduo::Timestamp now(muduo::Timestamp::now());
    //读取定时器事件
    readTimerfd(timerfd_, now);

    // 根据当前时间，得到过期的定时器
    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();

    for (auto it = expired.begin(); it != expired.end(); ++it) {
        it->second->run();
    }
    callingExpiredTimers_ = false;
    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(muduo::Timestamp now)
{
    LOG_DEBUG << "getExpired()";
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;
    Entry sentry = std::make_pair(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    //Entry sentry = std::make_pair(now, timers_.begin()->second);

    // 因为set中是升序排列，取到左起第一个大于当前时间的定时器的迭代器
    auto it = timers_.lower_bound(sentry);

    // 如果迭代器指向的定时器对象的时间比当前时间晚，或者迭代器指到了队列尾，都是符合条件的
    assert(it == timers_.end() || now < it->first);

    // 将过期的定时器都拷贝到expired中
    std::copy(timers_.begin(), it, back_inserter(expired));

    // 然后从timers_中删除这些过期的迭代器
    timers_.erase(timers_.begin(), it);

    // 遍历删除activeTimers_中的到期定时器
    // BOOST_FOREACH   改为了C++11的范围for语句
    /*
    for (auto entry : expired) {
        ActiveTimer timer(entry.second, entry.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
        (void)n;
    }
    */
    BOOST_FOREACH(Entry entry, expired)
    {
        ActiveTimer timer(entry.second, entry.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1); (void)n;
    }
    assert(timers_.size() == activeTimers_.size());
    return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, muduo::Timestamp now)
{
    //下一次到期时间
    muduo::Timestamp nextExpire;

    for (auto it = expired.begin(); it != expired.end(); ++it) {
        ActiveTimer timer(it->second, it->second->sequence());
        //如果定时器可以重复间隔执行
        if (it->second->repeat() 
            && cancelingTimers_.find(timer) == cancelingTimers_.end()) {  
            //重启定时器
            it->second->restart(now);
            //并插入到队列
            insert(it->second);
        } else {
            delete it->second;
        }
    }

    //如果定时器队列不为空，则获取到定时器队列中最早的时间
    if (!timers_.empty())
        nextExpire = timers_.begin()->second->expiration();

    //如果获取到的时间合法，则为队列下次的到期时间
    if (nextExpire.valid())
        resetTimerfd(timerfd_, nextExpire);
}

bool TimerQueue::insert(Timer* timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    //标记插入的定时器到期时间是不是最早的
    bool earliestChanged = false;

    //得到新的定时器到期时间
    muduo::Timestamp when = timer->expiration();
    auto it = timers_.begin();

    // 如果定时器队列为空，或者队列中最早的的定时器都没新的定时器早，那说明新的定时器必然是最早的
    if (it == timers_.end() || when < it->first)
    {
        earliestChanged = true;
    }

    {
        std::pair<TimerList::iterator, bool> result = timers_.insert(Entry(when, timer));
        assert(result.second);
        (void)result;
    }

    {
        std::pair<ActiveTimerSet::iterator, bool> result = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second);
        (void)result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}
