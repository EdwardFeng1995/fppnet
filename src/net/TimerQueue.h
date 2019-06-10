#include <set>
#include <vector>

#include <boost/noncopyable.hpp>

#include "../base/datetime/Timestamp.h"
#include "../base/thread/Mutex.h"
#include "Callbacks.h"
#include "Channel.h"

namespace muduo
{

class EventLoop;
class Timer;
class TimerId;

class TimerQueue : boost::noncopyable
{
public:
    TimerQueue(EventLoop* loop);
    ~TimerQueue();

    TimerId addTimer(const TimerCallback& cb, Timestamp when, double interval);
    
    // 注销定时器
    void cancel(TimerId timerId);
private:
    
    using Entry = std::pair<Timestamp, Timer*>;
    //小题大作，有个缺点，如果用户一直持有TimerId，会造成引用计数所占的内存无法释放
    //using Entry = std::pair<Timestamp, std::shared_ptr<Timer>>;
    using TimerList = std::set<Entry>;
    using ActiveTimer = std::pair<Timer*, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    // 在IO线程中完成修改定时器列表的工作
    // void addTimerInLoop(std::shared_ptr<Timer> timer);
    void addTimerInLoop(Timer* timer);

    // 在IO线程中完成注销定时器的工作
    void cancelInLoop(TimerId timerid);
    
    // 当定时器事件到的时候处理事件
    void handleRead();

    // 将所有到期的定时器取出
    std::vector<Entry> getExpired(Timestamp now);

    // 将所有到期的定时器根据当前时间进行重置，并重置定时器队列的到期时间
    void reset(const std::vector<Entry>& expired, Timestamp now);

    // 新的定时器插入到队列
    bool insert(Timer* timer);
    // bool insert(std::shared_ptr<Timer> timer);

    EventLoop* loop_;        //所处的事件循环
    const int timerfd_;      //申请的文件描述符，真正的定时器其实就一个，
                             //根据插入的定时器，会重置timerfd
    Channel TimerfdChannel_; //观察timerfd_事件的Channel
    TimerList timers_;       //根据到期时间排好序的定时器列表

    // for cancel()
    // callingExpiredTimers_和cancelingTimers_是为了应对“自注销”这种情况
    // 即在定时器回调中注销当前定时器
    bool callingExpiredTimers_;    /*atomic*/
    ActiveTimerSet activeTimers_;  // 和timers_保存的数据一样，只不过是按对象地址排序
                                  // 后者按到期时间排序
    ActiveTimerSet cancelingTimers_;
};

}
