#include "../src/net/EventLoop.h"
#include "../src/base/thread/Thread.h"
#include <thread>
#include <stdio.h>

void threadFunc()
{
  printf("threadFunc(): pid = %d, tid = %d\n",
         getpid(), muduo::CurrentThread::tid());

  fppnet::EventLoop loop;
  loop.loop();
}

int main()
{
  printf("main(): pid = %d, tid = %d\n",
         getpid(), muduo::CurrentThread::tid());

  fppnet::EventLoop loop;

  muduo::Thread thread(threadFunc);
  //std::thread thread1(threadFunc);
  thread.start();
  
  loop.loop();
  pthread_exit(NULL);
 // thread1.join();
}
