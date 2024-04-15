#include "Buffer.h"
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
/*
    从fd上读取数据， poller工作在LT模式，没读完会一直上报
    Buffer缓冲区是有大小的，但从fd上读数据却不知道tcp数据最终大小
*/
ssize_t Buffer::readFd(int fd, int* saveErrno){

    char extrabuf[65536] = {0}; //栈上内存空间 64k

    struct iovec vec[2];

    //先填充vec0， 如果这块够就不用下面的
    const size_t writable = writableBytes(); //这是buffer底层缓冲区剩余可写大小

    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; //不够64k空间就使用2个缓冲区，够就用一个
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if( n < 0){
        *saveErrno = errno;
    }else if( n <= writable ){
        //buffer空间足够存了
        writerIndex_ += n;
    }else{
        //extrabuf也有数据
            writerIndex_ = buffer_.size(); //writerIndex一定写满了
            append(extrabuf, n - writable); // writerIndex_开始写 n - writable大小数据
    }

    return n;
}

//往fd写，也就是从buffer读
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}