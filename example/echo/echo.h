#ifndef FPPNET_EXAMPLE_ECHO_ECHO_H
#define FPPNET_EXAMPLE_ECHO_ECHO_H

#include "../../src/net/TcpServer.h"

class EchoServer
{
public:
    EchoServer(fppnet::EventLoop* loop,
               const fppnet::InetAddress& listenAddr);

    void start() {
        server_.start();
    }
private:
    void onConnection(const fppnet::TcpConnectionPtr& conn);

    void onMessage(const fppnet::TcpConnectionPtr& conn,
                   fppnet::Buffer* buf,
                   muduo::Timestamp time);

    fppnet::EventLoop* loop_;
    fppnet::TcpServer server_;
};

#endif
