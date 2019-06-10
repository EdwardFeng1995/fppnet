#include "Acceptor.h"

#include "../base/logging/Logging.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "SocketsOps.h"

using namespace muduo;

// 构造函数和listen相当于创建服务端的传统步骤，socket(), bind(), listen()
// Acceptor不处理连接，交给acceptChannel负责连接事件的分发，
// 当有连接请求时，handleRead处理连接请求，实现连接，并调用新连接的回调
//
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie()),
      acceptChannel_(loop, acceptSocket_.Getfd()),
      listenning_(false)
{
    acceptSocket_.setResuAddr(true);
    acceptSocket_.BindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
    
}

// 开启监听，相当于打开电话，可以接收连接请求了
void Acceptor::listen()
{
    // 确保在IO线程
    loop_->assertInLoopThread();
    // 监听状态为真，电话是开着
    listenning_ = true;
    // 开启监听
    acceptSocket_.Listen();
    // acceptChannel_读事件使能，相当于将socketfd加入poll监听数组
    acceptChannel_.enableReading();
}

// acceptSocket_.fd 可读时的回调函数，意味有连接请求，可理解为有电话打来了
void Acceptor::handleRead()
{
    loop_->assertInLoopThread();
    InetAddress peerAddr(0);
    // 接受连接，创建连接后的文件描述符
    int connfd = acceptSocket_.Accept(&peerAddr);
    if (connfd >= 0) {
        // 如果设置了新连接的回调函数，执行回调
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
        } else {
            // 否则，关闭连接
            sockets::close(connfd);
        }
    }
}
