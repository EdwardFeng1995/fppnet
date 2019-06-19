#ifndef FPPNET_SRC_NET_CALLBACKS_H
#define FPPNET_SRC_NET_CALLBACKS_H

#include <memory>
#include <functional>
#include "../base/datetime/Timestamp.h"
#include "Buffer.h"

namespace fppnet
{

// All client visible callbacks go here.
class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

using TimerCallback = std::function<void()>;
using ConnectionCallback = std::function<void (const TcpConnectionPtr&)>;
using MessageCallback  = std::function<void (const TcpConnectionPtr&,
                                             Buffer* data,
                                             muduo::Timestamp)>;   // Timestamp是poll返回的时刻，即消息到达的时刻
                                                            // 早于读到数据的时刻(read调用或返回)
using CloseCallback = std::function<void (const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void (const TcpConnectionPtr&)>;
using HighWaterMarkCallback = std::function<void (const TcpConnectionPtr&, size_t)>;
}

#endif
