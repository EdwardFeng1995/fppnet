// 书上p303 练习2，同时监听两个port，每个port发生不同的字符串

#include "../src/net/Socket.h"
#include "../src/net/InetAddress.h"
#include "../src/net/EventLoop.h"
#include "../src/net/Acceptor.h"
#include "../src/net/Channel.h"
#include "../src/net/SocketsOps.h"
#include "../src/net/EventLoopThread.h"
using namespace fppnet;

void newConnection1(int sockfd, const InetAddress& peeraddr)
{
    printf("tid = %d, newConnection()1: accepted a new connection from %s\n",muduo::CurrentThread::tid(), peeraddr.toHostPort().c_str());
    ::write(sockfd, "How are you?\n", 13);
    sockets::close(sockfd);
}

void newConnection2(int sockfd, const InetAddress& peeraddr)
{
    printf("tid = %d, newConnection()2: accepted a new connection from %s\n", muduo::CurrentThread::tid(), peeraddr.toHostPort().c_str());
    ::write(sockfd, "How do you do?\n", 15);
    sockets::close(sockfd);
}

int main()
{
    printf("main(): pid = %d\n", getpid());

    InetAddress listenAddr1(9981);
    InetAddress listenAddr2(9982);

    // 创建两个EventLoop线程
    EventLoopThread loopThread1;
    EventLoopThread loopThread2;
    // 分别获取两个EventLoop线程的loop
    EventLoop* loop1 =loopThread1.startLoop();
    EventLoop* loop2 = loopThread2.startLoop();

    // 创建两个连接器
    Acceptor acceptor1(loop1, listenAddr1);
    Acceptor acceptor2(loop2, listenAddr2);

    // 分别设置两个新连接的回调函数
    acceptor1.setNewConnectionCallback(newConnection1);
    acceptor2.setNewConnectionCallback(newConnection2);
    
    // listen()要在IO线程调用
    loop1->runInLoop(std::bind(&Acceptor::listen, &acceptor1));
    loop2->runInLoop(std::bind(&Acceptor::listen, &acceptor2));
    
    while(1);
}

