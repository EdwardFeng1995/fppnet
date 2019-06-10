## muduo网络库设计与实现（4）

### Acceptor class

#### 1、SocketsOps.h

**封装了socket的操作**，包括字节序转换，创建socket，bind，listen，accept等，还将inet_ntop和inet_pton封装为toHostPort()和fromHostPort()

**socket知识回顾：**

以打电话为例：
（1）创建一个socket等价与安装一部电话。为使两个应用程序能够通信，每个应用程序都必须创建一个socket。

（2）bind()把socket绑定到一个众所周知的地址上，相当于电话绑定了电话号码。

（3）由socket()创建的socket是主动的，通过listen()，将socket变为被动的，通知内核它愿意接受接入连接的意愿，相当于打开电话，人们可以打进电话了。

（4）其他应用程序调用connect()相当于打电话。

（5）accept()，相当于电话响了，拿起电话建立连接，会返回一个已建立的文件描述符connfd，还可以得到对端地址（电话号码），如果对端没有connect()，accept()会阻塞（等电话）

将这些socket操作封装起来，可以将出错判断也封装起来，程序更简洁好用。

#### 2、InetAddress class

该类是对struct sockaddr_in的简单封装，能自动转换字节序，可拷贝。

#### 3、Socket class

该类是一个RAII handle，封装了socket文件描述符的生命期。

#### 4、Acceptor class

该类是一个连接器，用于accept(2)新TCP连接，并通过回调通知使用者，是内部class，供TcpServer使用，生命期由后者控制。

构造函数和listen相当于创建服务端的传统步骤，socket(), bind(), listen()。

Acceptor不处理连接，交给acceptChannel负责连接事件的分发，当有连接请求时，handleRead处理连接请求，调用accept(2)实现连接，并调用新连接的回调。

handleRead()的策略简单，每次accept(2)一个socket，另外还有两种实现策略：
* 一是每次循环accept(2)，直到没有新的连接到达；

* 二是每次尝试accept(2)N个新连接，N的值一般是10.

这两种做法适合短连接服务，而muduo是为长连接服务优化的，所以用了最简单的办法。

**<font color=#ff00ee>这几天体会到了面向对象编程的感觉，抽象为类，然后封装隐藏对象的属性和实现细节，仅对外公开接口和对象进行交互</font>**

还有就是function<void()> bind()等的使用

