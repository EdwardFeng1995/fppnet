#include "../src/net/TcpServer.h"
#include "../src/net/EventLoop.h"
#include "../src/net/InetAddress.h"
#include <stdio.h>

std::string message1;
std::string message2;
int sleepseconds = 5;

void onConnection(const fppnet::TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    printf("onConnection(): new connection [%s] from %s\n",
           conn->name().c_str(),
           conn->peerAddress().toHostPort().c_str());
    if (sleepseconds > 0) {
        ::sleep(sleepseconds);
    }
    conn->send(message1);
    conn->send(message2);
    conn->shutdown();
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

  buf->retrieveAll();
}

int main(int argc, char* argv[])
{
  printf("main(): pid = %d\n", getpid());

  int len1 = 100;
  int len2 = 200;

  if (argc > 2)
  {
    len1 = atoi(argv[1]);
    len2 = atoi(argv[2]);
  }

  message1.resize(len1);
  message2.resize(len2);
  std::fill(message1.begin(), message1.end(), 'A');
  std::fill(message2.begin(), message2.end(), 'B');

  fppnet::InetAddress listenAddr(9981);
  fppnet::EventLoop loop;

  fppnet::TcpServer server(&loop, listenAddr);
  server.setConnectionCallback(onConnection);
  server.setMessageCallback(onMessage);
  server.start();

  loop.loop();
}

