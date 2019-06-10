#include "TcpServer.h"

#include "../base/logging/Logging.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "SocketsOps.h"

#include <stdio.h>  // snprintf

using namespace muduo;


TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr) :
    loop_(loop),
    name_(listenAddr.toHostPort()),
    acceptor_(new Acceptor(loop, listenAddr)),
    threadPool_(new EventLoopThreadPool(loop)),
    started_(false),
    nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, 
                                        std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{

}

// 设置线程数
void TcpServer::setThreadNum(int numThreads)
{
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}
void TcpServer::start()
{
    if (!started_) {
        started_ = true;
        threadPool_->start();
    }

    if (!acceptor_->listening()) {
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 新连接的回调
// 执行调用顺序EventLoop::loop()---Poller::poll()---Channel::handleEvent()
//             ---Acceptor::handleRead()---TcpServer::newConnection() 
//  在有新连接时，从线程池threadPool_中取出一个loop给新连接，然后线程切换，新连接的事务
//  到新连接的io线程去做。
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 确保在IO线程调用
    loop_->assertInLoopThread();

    // 格式化生成唯一标示连接TcpConnection的的映射中的键值connectName
    char buf[32];
    snprintf(buf, sizeof buf, "#%d", nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO << "TcpServer::newConnection [" << name_
             << "] - new connection [" << connName
             << "] from " << peerAddr.toHostPort();

    InetAddress localAddr(sockets::getLocalAddr(sockfd));
    EventLoop* ioLoop = threadPool_->getNextLoop();
    // 构造新TcpConnection对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    // 生成映射
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);   
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

// 在连接断开时，因为TcpConnection会在自己的ioLoop线程调用removeConnection，
// 所以切换会TcpServer的loop_线程中去
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

// 在connections_.erase后，再切换回ioLoop线程去执行TcpConnection::connectDestroyed
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    loop_->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
             << "] - connections " << conn->name();
    size_t n = connections_.erase(conn->name());
    
    assert(n == 1);
    (void) n;
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
