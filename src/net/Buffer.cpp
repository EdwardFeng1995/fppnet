#include "Buffer.h"
#include "SocketsOps.h"
#include "../base/logging/Logging.h"

#include <errno.h>
#include <memory.h>
#include <sys/uio.h>

using namespace muduo;

ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    char extrabuf[65536];   // 分配一个额外的buf
    struct iovec vec[2];    // 两块iovec，一块指向Buffer， 一块指向extrabuf
    const size_t writabel = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writabel;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    const ssize_t n = readv(fd, vec, 2);    // read data into multiple buffers
    if (n < 0) {
        *savedErrno = errno;
    } else if (implicit_cast<size_t>(n) <= writabel) {
        writerIndex_ += n;
    } else {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writabel);
    }
    return n;
}
