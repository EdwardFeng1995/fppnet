#include "EventLoop.h"
#include "Channel.h"
#include "EPoller.h"
#include "Poller.h"
#include "TimerQueue.h"
#include "../base/logging/Logging.h"

#include <assert.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <signal.h>

using namespace muduo;

//__thread变量每一个线程有一份独立实体，
//各个线程的值互不干扰。可以用来修饰那些带有全局性且值可能变，但是又不值得用全局变量保护的变量。
__thread muduo::EventLoop* t_loopInThisThread = 0;
const int kPollTimeMs = 10000;

// 忽略SIGPIPE信号，
// 避免对方断开连接，本地继续写入，造成服务器意外退出
class IgnoreSigPipe
{
public:
    IgnoreSigPipe() {
        ::signal(SIGPIPE, SIG_IGN);
    }
};

IgnoreSigPipe initObj;

// 生成唤醒poller_的文件描述符
static int createEventfd()
{
    // 生成非阻塞的文件描述符
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_SYSERR << "Failed in eventfd";
        abort();
    }
    return evtfd;
}

muduo::EventLoop::EventLoop()
    :looping_(false),
    quit_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),
    epoller_(new EPoller(this)),
    timerQueue_(new TimerQueue(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_))
{
    //记住本对象所属的线程（threadId_）
    LOG_TRACE << " EventLoop created " << this << " in thread " << threadId_;
    //one loop per thread，因此检查当前线程是否已经创建了其他Eventloop对象
    if(t_loopInThisThread) {
        LOG_FATAL << "Another EventLoop " << t_loopInThisThread 
                  << " exits in this thread " << threadId_;
    }
    else {
        t_loopInThisThread = this;
    }

    //设置wakeupFd 读事件发生时的回调函数
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));

    wakeupChannel_->enableReading();

}

muduo::EventLoop::~EventLoop()
{
    assert(!looping_);          //检查looping_，断言循环不在运行
    ::close(wakeupFd_);
    t_loopInThisThread = NULL;  
}

void muduo::EventLoop::loop()
{
    assert(!looping_);          //断言循环不在运行
    assertInLoopThread();       //断言循环在创建时的线程
    looping_ = true;            
    quit_ = false;

    //调用poller->poll循环监听事件，并将活跃事件存入活跃channels列表，依次调用事件处理程序
    while(!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = epoller_->poll(kPollTimeMs, &activeChannels_);
        for(ChannelList::iterator it = activeChannels_.begin();
            it != activeChannels_.end(); ++it) {
            (*it)->handleEvent(pollReturnTime_);
        }
        doPendingFunctors();
    }

    LOG_TRACE << "EventLoop " << this << " stop looping";
    looping_ = false;
}

void EventLoop::abortNotInloopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

void EventLoop::quit()
{
    quit_ = true;

    // 如果当前线程不是创建EventLoop时的线程，则唤醒poller_，
    // 不然本EventLoop持续阻塞的话，就无法正常退出了
    if (!isInLoopThread()) {
        wakeup();
    }

    /*
     * 为什么EventLoop自己的IO线程调用Qiut()的时候不用唤醒呢？
     * 因为自己都能调用Quit()了。。。那就说明没有被阻塞，不用唤醒。。。
     * 但是自己调用好像没什么用
     * */
}

void EventLoop::updateChannel(Channel* channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    epoller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    LOG_INFO << "removeChannel被调用";
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    epoller_->removeChannel(channel);
}

//为了使IO线程在空闲时也能执行一些任务
//在I/O线程中执行某个回调函数，可以跨线程调用
void EventLoop::runInLoop(const Functor& cb)
{
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(cb);
    }
}

//可以跨线程
void EventLoop::queueInLoop(const Functor& cb)
{
    //将cb插入队列
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(cb);
    }

    //如果调用queueInLoop()的线程不是IO线程，那么唤醒IO线程，才能及时执行doPendingFunctors()
    //如果在IO线程调用queueInLoop()，比如在doPendingFunctors()中执行functors[i]() 时又调用了queueInLoop()
    //而此时正在调用pending functor，那么也必须唤醒，否则这些新加的cb就不能被及时调用了
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}


TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb)
{
    return timerQueue_->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, cb);
}

TimerId EventLoop::runEvery(double interval, const TimerCallback& cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(cb, time, interval);
}

void EventLoop::cancel(TimerId timerId)
{
    return timerQueue_->cancel(timerId);
}

// 唤醒函数，往唤醒的文件中写入活动的事件，让阻塞的poller_检测到
// 这样就能完成唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

//当wakeup()执行后，也即向wakeupFd写后，wakeupChannel_调用本函数
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

//该函数只会被当前IO线程调用
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    //不是简单地在临界区内依次调用Functor，而是把回调列表swap()到局部变量funtors中
    //这样一方面减小了临界区的长度（意味着不会阻塞其他线程调用queueInLoop()
    //另一方面也避免了死锁（因为Functor可能再调用queueInLoop())
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(size_t i = 0; i < functors.size(); ++i) {
        functors[i]();   //有可能再调用queueInLoop()
    }
    callingPendingFunctors_ = false;
}
