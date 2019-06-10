## muduo网络库设计与实现（3）

### EventLoop::runInLoop()函数

1、**runInLoop()函数功能：**

<font color = #ff00ee>在IO线程能执行某个用户任务回调。如果用户在当前IO线程调用这个函数，回调会同步进行；如果用户在其他线程调用runInLoop，cb会被加入队列，IO线程会被唤醒来调用这个Functor </font>

可以方便线程间调配任务，例如把TimerQueue的成员函数调用移到IO线程，可以不加锁的情况下保证线程安全。

2、**唤醒IO线程的方法：**

传统办法是用pipe(2)，IO线程始终监管此管道的readable事件，在需要唤醒的时候，其他线程往管道里写一个字，这样IO线程就从IO multiplexing阻塞调用中返回。

如今Linux有eventfd(2)，可以高效地唤醒，因为它不必管理缓冲区。

3、**唤醒的两种情况:**

(1)、如果调用queueInLoop()的线程不是IO线程，那么唤醒是必需的；

(2)、如果在IO线程调用queueInLoop，而此时正在调用pending functor，那么也必须唤醒。

只有在IO线程的事件回调中调用queueInLoop()才无须wakeup()。

4、在s01版中的TimerQueue::addTimer()只能在IO线程调用，因此EventLoop::runAfter()不是线程安全的。让addTimer调用runInLoop，把实际的操作转移到IO线程来做。

5、IO线程不一定是主线程，可以在任何一个线程创建并运行EventLoop。一个程序也可以不止一个IO线程，为了方便使用，定义EventLoopThread class。

EventLoopThread会启动自己的线程，并在其中运行EventLoop::loop().
