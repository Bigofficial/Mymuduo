#pragma once
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>
// 网络库底层缓冲区类型定义
/*


    --------kcheapprepend-------------read--------write
*/
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;   // 前面8字节用来记录缓冲区长度
    static const size_t kInitialSize = 1024; // 缓冲区大小

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend)
    {
    }

    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    const char *peek() const
    {
        return begin() + readerIndex_; // 返回缓冲区中可读数据的起始地址
    }

    // onMessage 会调用retrieveallasstring Buffer -> String
    void retrieve(size_t len)
    { 
        if (len < readableBytes())
        {
            // 读取了一部分
            readerIndex_ += len; // 还剩 加上len-》writerIndex
        }
        else
        { // len == reableBytes()
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend; // 把writerindex拉到kcheapprepend
    }

    // 把onmessage函数上报的函数buffer数据，转成String类型数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); // 应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len); // 把readablebytes读len
        retrieve(len);                   // 上面把缓冲区中可读的数据已经读取出来了，这里要对缓冲区进行复位操作
        return result;
    }

    // buffer_size - writerIndex_   和 len比较 小于就要扩容
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容
        }
        
        
    }

    //把data,data+len 内存上的数据，添加到writable缓冲区当中
    void append(const char* data, size_t len){
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char* beginWrite(){
        return begin() + writerIndex_;
    }
    //常对象调用
    const char* beginWrite() const{
        return begin() + writerIndex_;
    }


    //从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);
 // 通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);
private:
    char *begin()
    {
        // 调用迭代器的* = it.operator*() 返回的那个元素本身，这里要再取一个地址 it.operator*().operator&()
        return &*buffer_.begin(); // vector底层数组元素首地址，数组起始地址
    }

    const char *begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        /*
            kCheapPrepend | reader | writer |
            kCheapPrepend |        leh                 |
        */

       if(writableBytes() + prependableBytes() < len + kCheapPrepend){
            buffer_.resize(writerIndex_ + len); 
       }else{
            size_t readable = readableBytes();
            //将 readerindex到writerindex中的数据移动到kcp
            std::copy(begin() + readerIndex_,
            begin() + writerIndex_,
            begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
       }
    }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};