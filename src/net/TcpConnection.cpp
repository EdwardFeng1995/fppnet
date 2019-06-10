#include "TcpConnection.h"

#include "../base/logging/Logging.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include "SocketsOps.h"

#include <errno.h>
#include <stdio.h>
using namespace muduo;

TcpConnection::TcpConnection(EventLoop* loop, const std::string& name, int sockfd,
                             const InetAddress& localAddr, const InetAddress& peerAddr) :
    loop_(loop),
    name_(name),
    state_(kConnecting),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAddr)
{
    LOG_DEBUG << "TcpConnection::ctor[" <<  name_ << "] at " << this
              << " fd=" << sockfd;
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
}

TcpConnection::~TcpConnection()
{
    LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
              << " fd=" << channel_->Getfd();
    LOG_INFO << "test TcpConnection析构";
}

// 如果在非IO线程调用， 它会把message复制一份，传给IO线程中的sendInLoop()来发送。
void TcpConnection::send(const std::string& messgae)
{
    LOG_DEBUG << "TcpConnection::send()";
    if(state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(messgae);
        } else {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, messgae));
        }
    }
}

void TcpConnection::sendInLoop(const std::string& message)
{
    LOG_DEBUG << "TcpConnection::sendInLoop()";
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    // 如果输出队列没有数据，尝试直接写
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->Getfd(), message.data(), message.size());
        if(nwrote >= 0) {
            if (implicit_cast<size_t>(nwrote) < message.size()) {
                LOG_TRACE << "I am going to write more data";
            } else if (writeCompleteCallback_) {    // 调用用户设置的低水位回调函数
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_SYSERR << "TcpConnection::sendInLoop";
            }
        }
    }

    assert(nwrote >= 0);
    // 如果没有一次性全部写完，就把剩余数据写入outputBuffer_，然后关注可写事件
    if (implicit_cast<size_t>(nwrote) < message.size()) {
        outputBuffer_.append(message.data()+nwrote, message.size()-nwrote);
        if(!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown()
{
    LOG_DEBUG << "TcpConnection::shutdown()";
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    LOG_DEBUG << "TcpConnection::shutdownInLoop()";
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}

void TcpConnection::connectEstablished()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);
    channel_->enableReading();

    // std::enable_shared_from_this 能让一个对象（假设其名为 t ，且已被一个 std::shared_ptr 对象 pt 管理）
    // 安全地生成其他额外的 std::shared_ptr 实例（假设名为 pt1, pt2, ... ） ，它们与 pt 共享对象 t 的所有权。
    // // 若一个类 T 继承 std::enable_shared_from_this<T> ，
    // 则会为该类 T 提供成员函数： shared_from_this 。 
    // 当 T 类型对象 t 被一个为名为 pt 的 std::shared_ptr<T> 类对象管理时，
    // 调用 T::shared_from_this 成员函数，将会返回一个新的 std::shared_ptr<T> 对象，它与 pt 共享 t 的所有权。
    connectionCallback_(shared_from_this());  
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->Getfd(), &savedErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) {    // read返回0，关闭连接
        //LOG_INFO << "读到的是0， 调用handleClose()";
        handleClose();
    } else {
        errno = savedErrno;
        LOG_SYSERR << "TcpConnection::handleRead()";
        handleError();
    }
}

// 继续发送outputBuffer_中的数据。一旦发送完毕，立刻停止观察writeable事件，避免busy loop
void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if (channel_->isWriting()) {
        ssize_t n = ::write(channel_->Getfd(), outputBuffer_.peek(), outputBuffer_.readableBytes());
        if (n > 0) {
            // 可读位置标记移动
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();
                // 调用用户设置的低水位回调函数
                if (writeCompleteCallback_) {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                // 如果连接正在关闭，继续执行关闭过程
                if (state_ == kDisconnected) {
                    shutdownInLoop();
                }
            } else {
                LOG_TRACE << "I am going to write more data";
            }
        } else {
            LOG_SYSERR << "TcpConnection::handleWrite";
        }
    } else {
        LOG_TRACE << "Connection is down, no more writing";
    }
}

// 主要功能是调用closeCallback_，这个回调绑定到TcpServer::removeConnection()
void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpConnection::handleClose state = " << state_;
    LOG_INFO << "TcpConnection::handleClose state = " << state_;
    assert (state_ == kConnected || state_ == kDisconnecting);

    channel_->disableAll(); 
    closeCallback_(shared_from_this());
}

// 只是在日志中输入错误消息
void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->Getfd());
    LOG_ERROR << "TcpConnection::hadnleError [" << name_
              << "] - SO_ERROR = " << err << " " <<strerror_tl(err);
}

// TcpConnection析构前最后调用的一个成员函数，它通知用户连接已断开
void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnected || state_ == kDisconnecting);
    setState(kDisconnected);
    LOG_INFO << "TEST";
    
    channel_->disableAll();
    connectionCallback_(shared_from_this());
    
    loop_->removeChannel(channel_.get());
}

