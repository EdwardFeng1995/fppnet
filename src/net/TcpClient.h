#ifndef MUDUO_NET_TCPCLIENT_H
#define MUDUO_NET_TCPCLIENT_H

#include <boost/noncopyable.hpp>
#include <mutex>

#include "TcpConnection.h"

namespace muduo
{
class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient : boost::noncopyable
{
public:
    TcpClient(EventLoop* loop, const InetAddress& serverAddr);
    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    TcpConnectionPtr connection() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_;
    }

    bool retry() const;
    void enableRetry() {
        retry_ = true;
    }

    // 设置连接回调，不是线程安全的
    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }

    // 设置消息回调，不是线程安全的
    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }

    // 设置写完回调，不是线程安全的
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }

private:
    // Not thread safe, but in loop
    void newConnection(int sockfd);
    
    // Not thread safe, but in loop
    void removeConnection(const TcpConnectionPtr& conn);
    EventLoop* loop_;
    ConnectorPtr connector_;     // 避免覆盖Connector
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    bool retry_;   // atmoic;
    bool connect_; // atmoic;
    // always in loop thread;
    int nextConnId_;
    mutable std::mutex mutex_;
    TcpConnectionPtr connection_;
};

}
#endif
