#include <iostream>
#include "../src/net/EventLoop.h"
#include "../src/base/thread/Thread.h"

using namespace fppnet;

int main()
{
    EventLoop loop1;
    EventLoop loop2;
    loop1.loop();
    loop2.loop();
    return 0;
}

