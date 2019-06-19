#include "EventLoopThread.h"

#include "EventLoop.h"

using namespace fppnet;

EventLoopThread::EventLoopThread() :
    loop_(NULL),
    exiting_(false),
    thread_(std::bind(&EventLoopThread::threadFunc, this)),
    mutex_(),
    cond_()
{

}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  loop_->quit();
  thread_.join();
}

EventLoop* EventLoopThread::startLoop()
{
    // 互斥锁与条件变量配合使用，当新线程运行起来后才能够得到loop的指针
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 只要没有给loop_指针分配对象，就一直握着锁不放
        while (loop_ == NULL) {
            cond_.wait(lock);
        }
    }

    return loop_;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;
    
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_all();
    }

    loop.loop();
}
