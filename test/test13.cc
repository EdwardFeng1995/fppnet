#include "../src/net/Connector.h"
#include "../src/net/EventLoop.h"

#include <stdio.h>

fppnet::EventLoop* g_loop;

void connectCallback(int sockfd)
{
    printf("connected.\n");
    g_loop->quit();
}

int main(int argc, char* argv[])
{
    fppnet::EventLoop loop;
    g_loop = &loop;
    fppnet::InetAddress addr("127.0.0.1", 9981);
    fppnet::ConnectorPtr connector(new fppnet::Connector(&loop, addr));
    connector->setNewConnectionCallback(connectCallback);
    connector->start();

    loop.loop();
}

