#ifndef MUDUO_NET_TCPSERVER_H
#define MUDUO_NET_TCPSERVER_H
#include "Callbacks.h"
#include "TcpConnection.h"

#include <map>
#include <boost/noncopyable.hpp>

namespace muduo
{
class Acceptor;
class EventLoop;
class EventLoopThreadPool;

class TcpServer : boost::noncopyable
{
public:
    TcpServer(EventLoop* loop, const InetAddress& listenAddr);
    ~TcpServer();

    /// 设置线程数
    /// Set the number of threads for handling input.
    ///
    /// Always accepts new connection in loop's thread.
    /// Must be called before @c start
    /// @param numThreads
    /// - 0 means all I/O in loop's thread, no thread will created.
    ///   this is the default value.
    /// - 1 means all I/O in another thread.
    /// - N means a thread pool with N threads, new connections
    ///   are assigned on a round-robin basis.
    void setThreadNum(int numThreads);


    // 多次调用无害，线程安全的
    void start();

    // 设置连接回调， 不是线程安全
    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }

    // 设置信息回调， 不是线程安全的
    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }

private:
    // not thread safe, but in loop
    void newConnection(int sockfd, const InetAddress& peerAddr);

    // Thread safe
    void removeConnection(const TcpConnectionPtr& conn);

    // not thread safe, but in loop
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::map<std::string, TcpConnectionPtr>;

    EventLoop* loop_;                       // the acceptor loop

    const std::string name_;                // TcpConnection对象的名字,ConnectionMap的key

    std::unique_ptr<Acceptor> acceptor_;    // 服务器接收新连接的接收器

    std::unique_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_; // 用户使用的回调函数

    MessageCallback messageCallback_;

    WriteCompleteCallback writeCompleteCallback_;

    bool started_;                          // 是否开始运行

    int nextConnId_;                        // 下一个连接的索引

    ConnectionMap connections_;             // 根据名字设的连接的映射
};

}
#endif
