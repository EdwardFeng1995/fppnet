## muduo网络库实现（9）

### 多线程TcpServer

### EventLoopThreadPool

用one loop per thread的思想实现多线程TcpServer的关键步骤是在新建TcpConnection时从event loop pool里挑一个loop给TcpConnection用。**也就是说多线程TcpServer自己的EventLoop只用来接受新连接，而新连接会用其他EventLoop来执行IO。**

所以创建EventLoopThreadPool class。

TcpServer每次新建一个TcpConnection就会调用getNextLoop()来取得EventLoop，如果是单线程服务，就返回baseloop_。

新连接建立时，从EventLoopThreadPool取得ioLoop。

```CPP
ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
```

该句的作用是让TcpConnection的ConnectionCallback由ioLoop线程调用。

连接断开时，把原来的removeConnection()拆为两个，因为TcpConnection会在自己的ioLoop线程调用removeConnection()，所以需要把它移到TcpServer的loop_线程。

```CPP
ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
```
该句再次把connectDestroyed()移到ioLoop线程进行，是为了保证TCPConnection的ConnectionCallback始终在其ioLoop回调。

**<font color=#ff0000>TcpServer和TcpConnection的代码都只处理单线程的情况（甚至没有mutex成员），而我们借助EventLoop::runInLoop()并引入EvenLoopThreadPool来实现多线程TcpServer。ioLoop和loop_间的线程切换都发生在连接建立和断开的时候。</font>**

