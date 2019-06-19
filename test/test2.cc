#include "../src/net/EventLoop.h"
#include "../src/base/thread/Thread.h"

fppnet::EventLoop* g_loop;

void threadFunc()
{
  g_loop->loop();
}

int main()
{
  fppnet::EventLoop loop;
  g_loop = &loop;
  muduo::Thread t(threadFunc);
  t.start();
  t.join();
}
