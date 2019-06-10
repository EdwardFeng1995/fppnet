#include "../src/net/Socket.h"
#include "../src/net/InetAddress.h"
#include "../src/net/EventLoop.h"
#include "../src/net/Acceptor.h"
#include "../src/net/Channel.h"
#include "../src/net/SocketsOps.h"

using namespace muduo;

void newConnection(int sockfd, const InetAddress& peeraddr)
{
    printf("newConnection(): accepted a new connection from %s\n", peeraddr.toHostPort().c_str());
    ::write(sockfd, "How are you?\n", 13);
    sockets::close(sockfd);
}

int main()
{
    printf("main(): pid = %d\n", getpid());

    InetAddress listenAddr(9981);
    EventLoop loop;

    Acceptor acceptor(&loop, listenAddr);
    acceptor.setNewConnectionCallback(newConnection);
    acceptor.listen();
    
    loop.loop();
}

