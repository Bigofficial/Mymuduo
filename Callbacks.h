#pragma once

#include <memory>
#include <functional>
class Buffer;
class TcpConnection;
class Timestamp;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>; // 对端的信息 ip 端口
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;

using MessageCallback = std::function<void(const TcpConnectionPtr &,
                                           Buffer *,
                                           Timestamp)>;

// 水位控制就是要求收发两端的数据收发速度要一致
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;