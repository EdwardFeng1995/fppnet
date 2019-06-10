## muduo网络库实现（6）

### muduo Buffer类的设计与使用

### 1、muduo的IO模型

event loop是non-blocking网络编程的核心，在现实生活中，non-blocking几乎总是和IO multiplexing一起使用的，原因如下：

* 没有人真的会用轮询来检查某个非阻塞IO操作是否完成

* IO multiplexing一般不能和blocking IO用在一起，因为blocking IO中read()/write()/accept()/connect()都可能阻塞当前线程，线程就无法处理其他socket上的IO事件。

### 2、为什么non-blocking网络编程中应用层buffer是必需的

non-blocking IO的核心思想是避免阻塞子啊read()或write()或其他IO系统调用上，这样可以最大限度地复用thread-of-control，让一个线程服务于多个socket连接。IO 线程只会阻塞在IO multiplexing函数上，如select/poll/epoll，所以应用层的缓冲是必需的。每个socket都要有stateful的input buffer 和 output buffer。

**TcpConnection 必需要有output buffer**，为了使程序在write操作上不阻塞(p.205).

**TcpConnection 必须要有input buffer**。网络库在处理“socket可读”事件的时候，必须一次性把socket里的数据读完（从操作系统buffer搬到应用层buffer），否则会反复触发POLLIN事件，造成busy-loop。

muduo EventLoop采用的是epoll(4)level trigger，而不是edge trigger。

* 一是为了与传统poll(2)兼容，因为在文件描述符数目较少，活动文件描述符比例较高时，epoll(4)不见得比poll(2)更高效。
* 二是level trigger编程更容易，以往select(2)/poll(2)的经验都可以继续用。
* 三是读写的时候不必等候出现EAGAIN，可以节省系统调用次数，降低延迟。

muduo中不read或write某个socket，只会操作TcpConnection的input buffer 和 output buffer。

### 3、Buffer的功能需求

muduo Buffer的设计要点：
* 对外表现为一块连续的内存
* 其size()可以自动增长，以适应不同大小的消息。
* 内部以std::vector<char> 来保存数据

Buffer其实更像是一个queue，从末尾写入数据，从头部读出数据。

* input buffer，TcpConnection会从socket读取数据，然后写入input buffer(Buffer::readFd()完成)，客户代码从input buffer读取数据。
* output buffer，客户代码把数据写入output Buffer(TcpConnection::send()完成)，TcpConnection从output buffer读取数据并写入socket。

**在非阻塞网络编程中，如何设计并使用缓冲区？**

* 一方面希望减少系统调用，一次读的数据越多越划算。
* 另一方面希望减少内存占用

具体做法是，在栈上准备一个65536字节的extrabuf，然后利用readv()来读取数据，iovec有两块，第一块指向muduo Buffer的writable字节，另一块指向栈上的extrabuf。如果数据不多，全部读入Buffer；如果数据长度超过Buffer的writable字节数，就会读到栈上的extrabuf里，然后在append()到Buffer中。

由于muduo的事件触发采用level trigger，因此不会反复调用read()直到其返回EAGAIN，从而可以降低消息处理的延迟。

**线程安全？**
Buffer不是线程安全的：
* 对于input buffer，onMessage()回调始终发生在该TcpConnection所属的那个IO线程，应用程序应该在onMessage()完成对input buffer的操作，并且不要把input buffer暴露给其他线程。这样所有对input Buffer的操作都在同一个线程，Buffer class不必是线程安全的。

* 对于output buffer，应用程序不会直接操作它，而是调用TcpConnection::send()来发送数据的，后者是线程安全的。

### 4、Buffer的数据结构

**prependable、readable、 writable三个部分**，
**readerIndex_, writerIndex_划分为三块，指示位置。**

其余略。

