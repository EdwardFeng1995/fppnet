#ifndef FPPNET_SRC_NET_BUFFER_H
#define FPPNET_SRC_NET_BUFFER_H

#include "../base/datetime/copyable.h"
#include <algorithm>
#include <string>
#include <vector>

#include <assert.h>

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode

namespace fppnet
{

class Buffer : public muduo::copyable
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;
    Buffer()
        : buffer_(kCheapPrepend + kInitialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == kInitialSize);
        assert(prependableBytes() == kCheapPrepend);
    }
    
    size_t readableBytes() const
    { return writerIndex_ - readerIndex_; }

    size_t writableBytes() const
    { return buffer_.size() - writerIndex_; }

    size_t prependableBytes() const
    { return readerIndex_; }

    // 返回Buffer中可读给用户的区域的首指针
    const char* peek() const
    { return begin() + readerIndex_; }

    // 从Buffer中读len个字节后，可读标记后移，返还len字节空间
    void retrieve(size_t len) {
        assert(len <= readableBytes());
        readerIndex_ += len;
    }
    
    // 从Buffer中读直到end处后，可读标记移动end
    void retrieveUntil(const char* end) {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    // 从Buffer中读全部数据后，初始化位置标记
    void retrieveAll() {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 从Buffer中取出所有可读的字符串
    std::string retrieveAsString() {
        printf("DEBUG retrieveAsString\n");
        std::string str(peek(), readableBytes());
        retrieveAll();
        return str;
    }

    // 写入数据
    void append(const std::string& str)
    {
        append(str.data(), str.length());    // 重载
    }

    void append(const char* /*restrict*/ data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        hasWritten(len);
    }

    void append(const void* /*restrict*/ data, size_t len)
    {
        append(static_cast<const char*>(data), len);
    }

    // 确保可写len个字节
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len) {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    // 可写的首指针
    char* beginWrite()
    { return begin() + writerIndex_; }

    const char* beginWrite() const
    { return begin() + writerIndex_; }

    // 已经写了len字节，可写首指针后移len
    void hasWritten(size_t len)
    { writerIndex_ += len; }

    // 前方添加数据
    void prepend(const void* /*restrict*/ data, size_t len) {
        assert(len <= prependableBytes());
        readerIndex_ -= len;
        const char* d = static_cast<const char*>(data);
        std::copy(d, d+len, begin()+readerIndex_ );
    }

    // 扩容
    void shrink(size_t reserve) {
        std::vector<char> buf(kCheapPrepend + readableBytes() + reserve);
        std::copy(peek(), peek() + readableBytes(), buf.begin() + kCheapPrepend);
        buf.swap(buffer_);
    }

    // 供外界调用的功能函数，读取socket内容
    // ssize_t 为有符号的，32位机上是int，64位机上是long int
    // size_t 为无符号的
    ssize_t readFd(int fd, int* savedErrno);

private:
    char* begin() {
        return &*buffer_.begin();
    }

    const char* begin() const {
        return &*buffer_.begin();
    }

    // 制造len的空间
    void makeSpace(size_t len) {
        // 可以内部腾挪，不够的情况下先将已有数据腾挪到prepend区域，所以这里是w+p
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
            buffer_.resize(writerIndex_ + len);
        } else {
            // 内部腾挪，将已有数据移动到prepend区域，腾出writeable空间
            assert(kCheapPrepend < readerIndex_);
            size_t readable = readableBytes();
            std::copy(begin()+readerIndex_, begin()+writerIndex_, begin()+kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
            assert(readable == readableBytes());
        }
    }

private:
    std::vector<char> buffer_;     //Buffer的核心数据结构，连续的内存
    size_t readerIndex_;
    size_t writerIndex_;
};

}
#endif
