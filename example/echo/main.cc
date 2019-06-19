#include "echo.h"

#include "../../src/base/logging/Logging.h"
#include "../../src/net/EventLoop.h"

using namespace fppnet;

int main()
{
    LOG_INFO << "pid = " << getpid();
    EventLoop loop;
    InetAddress listenAddr(2007);
    EchoServer server(&loop, listenAddr);
    server.start();
    loop.loop();
}
