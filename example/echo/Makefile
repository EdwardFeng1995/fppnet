LIB_SRC = 			\
	../../src/net/Acceptor.cpp		\
	../../src/net/Buffer.cpp		\
	../../src/net/Channel.cpp		\
	../../src/net/Connector.cpp		\
	../../src/net/EPoller.cpp	 	\
	../../src/net/EventLoop.cpp		\
	../../src/net/EventLoopThread.cpp	\
	../../src/net/EventLoopThreadPool.cpp	\
	../../src/net/InetAddress.cpp		\
	../../src/net/Poller.cpp		\
	../../src/net/Socket.cpp		\
	../../src/net/SocketsOps.cpp		\
	../../src/net/TcpClient.cpp		\
	../../src/net/TcpConnection.cpp	\
	../../src/net/TcpServer.cpp		\
	../../src/net/Timer.cpp		\
	../../src/net/TimerQueue.cpp \
	./echo.cpp

BINARIES = main

all: $(BINARIES)

include ./reactor_lib.mk

main: ./main.cc