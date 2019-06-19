#include "echo.h"

#include "../../src/base/logging/Logging.h"

#include <functional> 

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

using namespace fppnet;
using namespace muduo;

EchoServer::EchoServer(fppnet::EventLoop* loop, const fppnet::InetAddress& listenAddr) :
    loop_(loop),
    server_(loop, listenAddr)
{
    server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, _1, _2, _3));
}

void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
    LOG_INFO << "EchoServer - " << conn->peerAddress().toHostPort() << " -> "
             << conn->localAddress().toHostPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");
}

void EchoServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           muduo::Timestamp time)
{
  muduo::string msg(buf->retrieveAsString());
  LOG_INFO << conn->name() << " echo " << msg.size() << " bytes, "
           << "data received at " << time.toString();
  conn->send(msg);
}

