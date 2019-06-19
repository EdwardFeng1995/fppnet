#ifndef FPPNET_SRC_NET_SOCKET_H
#define FPPNET_SRC_NET_SOCKET_H

#include <boost/noncopyable.hpp>

namespace fppnet
{

class InetAddress;

class Socket
{
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) 
    {  }

    ~Socket();

    int Getfd()
    {
        return sockfd_;
    }
    
    // 绑定本地地址
    void BindAddress(const InetAddress& localaddr);

    void Listen();

    // peeraddr 传入传出 获得对端地址
    int Accept(InetAddress* peeraddr);

    // Enable/disable SO_REUSEADDR
    // SO_RESUEADDR 设置端口复用
    void setResuAddr(bool on);

    void shutdownWrite();

    void setTcpNoDelay(bool on);

    void setKeepAlive(bool on);

private:
    const int sockfd_;
};

}
#endif
