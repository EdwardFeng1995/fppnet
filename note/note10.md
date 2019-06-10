## muduo网络库实现（10）

### 1、Connector

主动发起连接比被动接受连接要复杂一些，一方面是**错误处理麻烦**，另一方面是**要考虑重试**。在非阻塞网络编程中，发起连接的基本方式是调用connect(2)，当socket变得可写时表明连接建立完毕。

Connector只负责建立socket连接，不负责创建TcpConnection。

**Connector的实现有几个难点：**
* **socket是一次性的**，一旦出错（比如对方拒绝连接），就无法恢复，**只能关闭重来**。但Connector是可以反复使用的，因此每次尝试连接都使用新的socket文件描述符和新的Channel对象。要留意Channel对象的生命期管理，并防止socket文件描述符泄露。

* **错误代码与accept(2)不同**，EAGAIN是真的错误，表明本机ephemeral(临时) port暂时用完，要关闭socket再延期重试。“正在连接”的返回码是**EINPROGRESS**。另外，即便socket可写，也不一定意味着连接已成功建立，还需要**getsockop(sockfd, SOL_SOCKET, SO_ERROR, ...)再次确认一下**。

* **重试的间隔应该逐渐延长**，例如0.5s、1s、2s、4s，直至30s，即back-off。如果使用EventLoop::runAfter()定时而Connector在定时器到期之前析构了怎么办？此处采用的做法是在Connector的析构函数中注销定时器。

* **要处理自连接(self-connection)**。如果destination IP正好是本机，而destination port位于local port range，且没有服务程序监听的话，ephemeral port正好选中了destination port，就会出现(sourec IP,sourec port) = (destination IP,destination port)的情况，即发生了自连接。处理办法是断开连接再重试，否则原本侦听destination port的服务程序也无法启动了。

**<font color=#ff0000>本来在之前的设计中，把Timer* 改成了 std::shared_ptr<Timer>，方便内存管理，但是有点小题大用，这种做法也有一个缺点，如果用户一直持有TimerId，会造成引用计数所占的内存无法释放。现在让TimerId包含Timer*，因为无法区分地址相同的先后两个Timer对象。因此每个Timer对象有一个全局递增的序列号int64_t sequence_，TimerId同时保存Timer*和sequence_，这样TimerQueue::cancel()就能根据TimerId找到需要注销的Timer对象。</font>**

TimerQueue新增了几个数据成员，**activeTimers_**保存的是目前有效的Timer指针，并且 **timer_.size() == activeTimers_.size()**，这两个容器保存的是相同的数据，只不过times_是按到期时间排序，activeTimers_是按对象地址排序。

由于TimerId不负责Timer的生命期，其中保存的Timer*可能失效，因此不能直接dereference，只有在activeTimers_中找到了Timer时才能提领。

### 2、TcpClient

* **TcpClient具备TcpConnection断开之后重新连接的功能**，Connector具备反复尝试连接的功能，因此客户端和服务端的启动顺序无关紧要。服务端可以重启，客户端也能重连。

* **连接断开后初次重连应该有随机性**，每个TcpClient应该等待一段随机的时间（0.5s～2s），再重试，避免拥塞。

* **发起连接的时候如果发生Tcp SYN丢包，那么系统默认的重试间隔是3s，这期间不会返回错误码，而且这个间隔似乎不容易修改。**

* 目前实现的TcpClient没有充分测试动态增减的情况，也就是说没有充分测试TcpClient的生命期比EventLoop短的情况，特别是没有充分测试TcpClient在连接建立期间析构的情况。

### 3、epoll

epoll(4)与poll(2)的不同之处主要在于poll(2)每次返回整个文件描述符数组，用户代码需要遍历数组以找到哪些文件描述符上有IO事件，而epoll_wait(2)返回的是活动fd的列表，需要遍历的数组通常会小得多。**在并发连接数较大而活动连接比例不高时，epoll(4)比poll(2)更高效**。

EPoller中的events_不是保存所有关注的fd列表，而是一次epoll_wait(2)调用返回的活动fd列表，它的大小是自适应的。