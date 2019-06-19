#ifndef FPPNET_SRC_NET_CHANNEL_H
#define FPPNET_SRC_NET_CHANNEL_H

#include <boost/noncopyable.hpp>
#include <functional>     //标准库的function
#include "../base/datetime/Timestamp.h"

namespace fppnet
{

class EventLoop;

/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.
/// The file descriptor could be a socket,
/// an eventfd, a timerfd, or a signalfd
class Channel : boost::noncopyable
{
public:
    /*
     * std::function 的实例能存储、复制及调用任何可调用 (Callable) 目标
     * ——函数、 lambda 表达式、 bind 表达式或其他函数对象，
     * 还有指向成员函数指针和指向数据成员指针。
     * 用function代替函数指针，实现函数回调，灵活性更高
     */
    //typedef boost::function<void()> EventCallback;
    using EventCallback = std::function<void()>;       //使用using写法以及使用标准库的function
    using ReadEventCallback = std::function<void(muduo::Timestamp)>;

    // 构造函数
    Channel(EventLoop* loop, int fd);

    // 事件分发
    void handleEvent(muduo::Timestamp receiveTime);

    // 设置读、写、错误的回调函数
    void setReadCallback(const ReadEventCallback& cb) {
        readCallback_ = cb;
    }
    void setWriteCallback(const EventCallback& cb) {
        writeCallback_ = cb;
    }
    void setErrorCallback(const EventCallback& cb) {
        errorCallback_ = cb;
    }
    void setCloseCallback(const EventCallback& cb) {
        closeCallback_ = cb;
    }

    // 获取当前文件描述符
    int Getfd() const {
        return fd_;
    }

    //获取当前关注的事件
    int Getevents() const {
        return events_;
    }

    //设置活动的事件
    void set_revents(int revt) {
        revents_ = revt;
    }

    //判断本Channel关注的事件是否为空
    bool isNoneEvent() const {
        return events_ == kNoneEvent;
    }
    
    void enableReading() {
        events_ |= kReadEvent;
        update();
    }

    void enableWriting() {
        events_ |= kWriteEvent;
        update();
    }

    void disableWriting() {
        events_ &= ~kWriteEvent;
        update();
    }

    void disableAll() {
        events_ = kNoneEvent;
        update();
    }

    bool isWriting() const {
        return events_ & kWriteEvent;
    }

    //Poller可以调用
    int index() {
        return index_;
    }


    void set_index(int index) {
        index_ = index;
    }

    EventLoop* ownerLoop() {
        return loop_;
    }

    

    ~Channel();

private:
    void update();

    // 所有Channel对象都共享事件的值
    // 无论在哪里，一种事件对应的值是一样的，设置为static可以节省空间
    // static常量的定义（与声明区分）放到源文件中
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_; //所在的loop，每个Channel对象自始至终只属于一个EventLoop
    const int fd_;    //Channel对象对应的fd，每个Channel对象始终只负责一个文件描述符IO事件分发
    int events_;      //关心的IO事件，由用户设置
    int revents_;     //目前活动的事件，由EventLoop/Poller设置
    int index_;       //used by Poller，记录自己在pollfds_数组中的下标
    bool eventHandling_; //事件是否正在处理

    ReadEventCallback readCallback_;    
    EventCallback writeCallback_;
    EventCallback errorCallback_;
    EventCallback closeCallback_;
};

}
#endif
