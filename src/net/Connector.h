#ifndef MUDUO_NET_CONNECTOR_H
#define MUDUO_NET_CONNECTOR_H

#include "InetAddress.h"
#include "TimerId.h"

#include <memory>
#include <functional>
#include <boost/noncopyable.hpp>

namespace muduo
{

class Channel;
class EventLoop;

class Connector : boost::noncopyable
{
public:
    using NewConnectionCallback = std::function<void (int sockfd)>;
    Connector(EventLoop* loop, const InetAddress& serverAddr);
    ~Connector();

    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

    void start();     // can be called in any thread
    void restart();   // must be called in loop thread
    void stop();      // can be called in any thread

    const InetAddress& serverAddress() const {
        return serverAddr_;
    }

private:
    enum States { kDisconnected, kConnecting, kConnected };
    static const int kMaxRetryDelayMs = 30*1000;    // 最大重连延时
    static const int kInitRetryDelayMs = 500;       // 初始重连延时

    void setState(States s) {
        state_ = s;
    }

    void startInLoop();          // 在IO线程启动
    void connect();              // connect操作，封装了创建socket和connect操作
    void connecting(int sockfd); // 设置Channel
    void handleRead();           // 读回调
    void handleWrite();          // 写回调
    void handleError();          // 出错回调

    void retry(int sockfd);
    int removeAndResetChannel();
    void resetChannel();

    EventLoop* loop_;
    InetAddress serverAddr_;
    bool connect_;
    States state_;
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;
    int retryDelayMs_;
    TimerId timerId_;
};
using ConnectorPtr = std::shared_ptr<Connector>;
}
#endif
