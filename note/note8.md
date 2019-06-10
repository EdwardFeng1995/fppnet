## muduo网络库实现（8）

### 完善 TcpConnection

### 1、SIGPIPE

SIGPIPE的默认行为是终止进程，在命令行程序中这是合理的，但是在网络编程中，这意味着如果对方断开连接而本地继续写的话，会造成服务进程意外退出。

假如服务进程繁忙，没有及时处理对方断开连接的事件，就有可能出现在连接断开之后继续发送数据的情况。

**解决方法很简单，在程序开始的时候就忽略SIGPIPE，可以用C++全局对象做到这一点。**

### 2、TCP No Delay 和 TCP keepalive

TCP No Delay 和 TCP keepalive都是常用的TCP选项，前者的作用是禁用Nagle算法，避免连续发包出现延迟，这对编写低延迟网络服务很重要。后者的作用是定期探查TCP连接是否还存在。一般来说如果有应用层心跳的话，TCP keepalive不是必需的，但一个通用的网络库应该暴露其接口。

### 3、WriteCompleteCallback 和 HighWaterMarkCallback

非阻塞网络编程的发送数据比读取数据要困难得多：

* 一方面是“什么时候关注writeable事件”的问题，带来编码方面的难度
* 另一方面是如果发送数据的速度高于对方接收数据的速度，会造成数据在本地内存中堆积，这带来设计及安全性方面的难度。

针对第二个问题，木朵提供两个回调，“高水位回调”和“低水位回调”，muduo使用HighWaterMarkCallback和WriteCompleteCallback这两个名字。

WriteCompleteCallback很容易理解，如果发送缓冲区被清空，就调用它。

如果输出缓冲的长度超过用户指定的大小，就会触发回调（只在上升沿触发一次）。