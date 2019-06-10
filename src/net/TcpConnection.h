#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include <boost/noncopyable.hpp>
#include <string>
#include <memory>

#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"

namespace  muduo
{

class Channel;
class EventLoop;
class Socket;

class TcpConnection : boost::noncopyable,
                     public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
                  const std::string& name,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr_
                  );
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() { return localAddr_; }
    const InetAddress& peerAddress() { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }
    // TcpServer 收到一个新连接时调用
    void connectEstablished();   // 应该只能调用一次

    // 仅内部使用
    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }

    // 当TcpServer把TcpConnection从它的map中删除的时候调用
    void connectDestroyed();     // 应该只能调用一次

    //void send(const void* message,size_t len);
    // Thread safe.
    void send(const std::string& message);
    // Thread safe.
    void shutdown();

    // 关闭Nagle算法，避免连续发包出现延迟
    void setTcpNoDelay(bool on);

private:
    enum StateE { kConnecting, kConnected, kDisconnecting,  kDisconnected };

    void setState(StateE s) {
        state_ = s;
    }
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const std::string& message);
    void shutdownInLoop();

    EventLoop* loop_;
    std::string name_;
    StateE state_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    InetAddress localAddr_;
    InetAddress peerAddr_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
    WriteCompleteCallback writeCompleteCallback_;    // 低水位回调
    HighWaterMarkCallback highWaterMarkCallback_;    // 高水位回调
    size_t highWaterMark_;  // 高水位标记
    Buffer inputBuffer_;
    Buffer outputBuffer_;
};

}
#endif
