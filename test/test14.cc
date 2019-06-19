#include "../src/net/EventLoop.h"
#include "../src/net/InetAddress.h"
#include "../src/net/TcpClient.h"

#include "../src/base/logging/Logging.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>

std::string message = "Hello\n";

void onConnection(const fppnet::TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    printf("onConnection(): new connection [%s] from %s\n",
           conn->name().c_str(),
           conn->peerAddress().toHostPort().c_str());
    conn->send(message);
  }
  else
  {
    printf("onConnection(): connection [%s] is down\n",
           conn->name().c_str());
  }
}

void onMessage(const fppnet::TcpConnectionPtr& conn,
               fppnet::Buffer* buf,
               muduo::Timestamp receiveTime)
{
  printf("onMessage(): received %zd bytes from connection [%s] at %s\n",
         buf->readableBytes(),
         conn->name().c_str(),
         receiveTime.toFormattedString().c_str());

  printf("onMessage(): [%s]\n", buf->retrieveAsString().c_str());
}

int main()
{
  fppnet::EventLoop loop;
  fppnet::InetAddress serverAddr("localhost", 9981);
  fppnet::TcpClient client(&loop, serverAddr);

  client.setConnectionCallback(onConnection);
  client.setMessageCallback(onMessage);
  client.enableRetry();
  client.connect();
  loop.loop();
}


