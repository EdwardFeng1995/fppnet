#include "Connector.h"

#include "Channel.h"
#include "EventLoop.h"
#include "SocketsOps.h"

#include "../base/logging/Logging.h"

#include <errno.h>

using namespace fppnet;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr) :
    loop_(loop),
    serverAddr_(serverAddr),
    connect_(false),
    state_(kDisconnected),
    retryDelayMs_(kInitRetryDelayMs)
{
    LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector()
{
    LOG_DEBUG << "dtor[" << this << "]";
    loop_->cancel(timerId_);     // Connector在定时器到期之前析构，注销定时器
    assert(!channel_);
}

void Connector::start()
{
    connect_ = true;
    loop_->runInLoop(std::bind(&Connector::startInLoop, this));   // 在IO线程去执行connect操作
}

void Connector::startInLoop()
{
    loop_->assertInLoopThread();
    assert(state_ == kDisconnected);      // 确保没有连接，避免重复连接
    if (connect_) {
        connect();
    } else {
        LOG_DEBUG << "do not connect";
    }
}

// 每次尝试连接都要使用心得socket文件描述符和心得Channel对象。
void Connector::connect()
{
    int sockfd = sockets::createNonblockingOrDie();
    int ret = sockets::connect(sockfd, serverAddr_.getSockAddrInet());
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno) {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
        connecting(sockfd);
        break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
        retry(sockfd);
        break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
        LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
        sockets::close(sockfd);
        break;

    default:
        LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
        sockets::close(sockfd);
        // connectErrorCallback_();
        break;
    }
}

void Connector::restart()
{
    loop_->assertInLoopThread();
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

void Connector::stop()
{
    connect_ = false;
    loop_->cancel(timerId_);
}

void Connector::connecting(int sockfd)
{
    setState(kConnecting);
    assert(!channel_);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(std::bind(&Connector::handleWrite, this));
    channel_->setErrorCallback(std::bind(&Connector::handleError, this));
    channel_->enableWriting();
}

int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    loop_->removeChannel(channel_.get());
    int sockfd = channel_->Getfd();
    
    // 不能在这里重置channel_，因为正在Channel::handleEvent()中
    loop_->queueInLoop(std::bind(&Connector::resetChannel, this));
    return sockfd;
}

void Connector::resetChannel()
{
    loop_->assertInLoopThread();
    channel_.reset();
}

void Connector::handleWrite()
{
    loop_->assertInLoopThread();

    LOG_TRACE << "Connector::handleWrite " << state_;

    if (state_ == kConnecting) {
        // 不在关注可写事件，将channel_从poller移除，并将channel_置空，防止loop busy
        int sockfd = removeAndResetChannel();

        // socket可写并不意味着连接一定建立成功
        // 还需要用getsockopt(sockfd, SOL_SOCKET, SO_ERROR, ...)再次确认一下。
        int err = sockets::getSocketError(sockfd);
        if (err) {           //有错误
            LOG_WARN << "Connector::handleWrite - SO_ERROR = " << err << " " << muduo::strerror_tl(err);
            retry(sockfd);   // 重连
        } 
        else if (sockets::isSelfConnect(sockfd)) {  // 发生了自连接，即源IP，源port=目的IP，目的port，断开重连
            LOG_WARN << "Connector::handleWrite - Self connect";
            retry(sockfd);   // 重连
        } 
        else {    // 连接成功
            setState(kConnected);
            if (connect_) {
                newConnectionCallback_(sockfd);
            } 
            else {
                sockets::close(sockfd);
            }
        }
    } 
    else {
        assert(state_ == kDisconnected);
    }
}

void Connector::handleError()
{
    LOG_ERROR << "Connector::handleError";
    assert(state_ == kConnecting);

    int sockfd = removeAndResetChannel();
    int err = sockets::getSocketError(sockfd);
    LOG_TRACE << "SO_ERROR = " << err << " " << muduo::strerror_tl(err);
    retry(sockfd);
}

// 采用back-off策略重连，即重连时间逐渐延长，0.5s, 1s, 2s, ...直至30s
void Connector::retry(int sockfd)
{
    // socket是一次性的，一旦出错，就无法恢复，只能关闭重来
    sockets::close(sockfd);
    setState(kDisconnected);
    if (connect_) {
        LOG_INFO << "Connector::retry - Retry connecting to "
                 << serverAddr_.toHostPort() << " in "
                 << retryDelayMs_ << " milliseconds. ";
        timerId_ = loop_->runAfter(retryDelayMs_/1000.0, std::bind(&Connector::startInLoop, this));
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    } else {
        LOG_DEBUG << "do not connect";
    }
}
