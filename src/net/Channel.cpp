#include "Channel.h"
#include "EventLoop.h"
#include "../base/logging/Logging.h"

#include <sstream>
#include <poll.h>

using namespace muduo;

const int Channel::kNoneEvent  = 0;
const int Channel::kReadEvent  = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd): 
      loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      eventHandling_(false)
{}

Channel::~Channel()
{
    assert(!eventHandling_);
}

//调用EventLoop::updateChannel()，后者会调用Poller::updateChannel()
void Channel::update()
{
    loop_->updateChannel(this);
}

//Channel核心，事件分发程序，由EventLoop::loop()调用
//功能是根据revents_的值分别调用不同的用户回调
void Channel::handleEvent(Timestamp receiveTime)
{
    eventHandling_ = true;
    if (revents_ & POLLNVAL) {
        LOG_WARN << "Channel::handleEvent() POLLNVAL";        
    }

    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        LOG_WARN << "Channel::handle_event() POLLHUP";
        if (closeCallback_) closeCallback_();
    }

    if (revents_ & (POLLERR | POLLNVAL)) {
        if (errorCallback_) errorCallback_();
    }

    if(revents_ & (POLLIN |POLLPRI | POLLHUP)) {
        if(readCallback_) readCallback_(receiveTime);
    }

    if(revents_ & POLLOUT) {
        if(writeCallback_) writeCallback_();
    }

    eventHandling_ = false;
}
