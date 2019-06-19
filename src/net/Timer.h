#ifndef FPPNET_SRC_NET_TIMER_H
#define FPPNET_SRC_NET_TIMER_H

#include <boost/noncopyable.hpp>

#include "../base/datetime/Timestamp.h"
#include "../base/thread/Atomic.h"
#include "Callbacks.h"

namespace fppnet
{

///
/// Internal class for timer event.
///
class Timer : boost::noncopyable
{
 public:
  Timer(const TimerCallback& cb, muduo::Timestamp when, double interval)
    : callback_(cb),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0),
      sequence_(s_numCreated_.incrementAndGet())
  { }

  void run() const
  {
    callback_();
  }

  muduo::Timestamp expiration() const  { return expiration_; }

  // 这个函数表示的是当前定时器是不是设置了间隔执行，即执行一次以后，下次的执行就是interval以后
  // 这个函数就是判断这个定时器能不能再次间隔运行
  bool repeat() const { return repeat_; }
  
  int64_t sequence() const { return sequence_; }
  void restart(muduo::Timestamp now);

 private:
  //用于接受回调函数
  const TimerCallback callback_;
  //期望的时间
  muduo::Timestamp expiration_;
  //间隔
  const double interval_;
  //是否重复
  const bool repeat_;

  const int64_t sequence_;  // 递增序列号，以便区分地址相同的先后两个Timer对象

  static muduo::AtomicInt64 s_numCreated_;
};

}
#endif  // MUDUO_NET_TIMER_H
