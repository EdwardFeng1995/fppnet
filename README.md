# fppnet：A High Performance C++ Network Library

## 简介

fppnet是一个仿照muduo（木铎）网络库实现的基于Reactor以及非阻塞IO的C++网络库，整体思路与muduo大致相同。

## 要点

* 基于Reactor模式，使用epoll水平触发的IO multiplexing，非阻塞IO

* 核心是事件循环EventLoop，用于响应计时器和IO事件。

* 采用基于对象（object-based）而非面向对象（object-oriented）的设计风格

* 回调接口多以std::function + std::bind表达

* 线程模型为：one loop per thread + thread pool

* 使用了C++智能指针管理对象，减少内存泄露的可能

* 可以多线程处理连接，主线程只负责连接请求，使用Round robin的方式分发给其他IO线程处理

* 待续

## 开发环境

* 操作系统：deepin15.8桌面版

* 编辑器：Vim + VS code

* 编译器：g++7.3.0

* 版本控制：git

## 网络核心库

##### 用户可见类：


| 类             |       描述         |
| :------------   | :--------------  |
| Buffer.{h,cpp} | 缓冲区，非阻塞IO必备 |
|  Callbacks.h   | 回调函数类型声明                  |
| Channel.{h,cpp} | 连接器，用于客户端发起连接 |
| EventLoop.{h,cpp} | 事件分发器 |
| EventLoopThread.{h,cpp} |新建一个专门用于EventLoop的线程 |
| EventLoopThreadPool.{h,cpp} | 默认多线程模型 |
| InetAddress.{h,cpp} | IP地址的简单分装 |
| TcpClinet.{h,cpp} | Tcp客户端 |
| TcpConnection.{h,cpp} | 封装了一次Tcp连接 |
| TcpServer.{h,cpp} | Tcp服务端 |
| TimerId.h | |

| 类             |       描述         |
| :------------   | :--------------  |
| Acceptor.{h,cpp} | 接受器，用于服务端接受连接 |
| Connector.{h,cpp} | 连接器，用于客户端发起连接 |
| Poller.{h,cpp} | poll IO multiplexing的基类接口|
| Epoller.{h,cpp} | epoll IO multiplexing的基类接口 |
| Socket.{h,cpp} | 封装Sockets描述符，负责关闭连接 |
| SocketsOps.{h,cpp} | 封装底层的Socket API |
| Timer.{h,cpp} | 定时器类 |
| TimerQueue.{h,cpp} | 定时器队列，用timerfd实现定时 |

待续