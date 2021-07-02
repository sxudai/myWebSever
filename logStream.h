#pragma once

#include <string>
#include <string.h>
#include <algorithm>
#include <stddef.h>

// buffer大小设置
// google C++ style: 常量k开头
const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000 * 1000;

// 一个固定大小的buffer
template <int SIZE> //非类型模板参数，不是传递类型而是传递值
class FixedBuffer{
private:
    char *cur_;  // 缓冲区尾指针
    char data_[SIZE];  // 真实缓冲区
    const char* end() const { return data_ + sizeof(data_);}  // 知道data的尽头
public:
    // 禁止拷贝构造
    FixedBuffer(const FixedBuffer &) = delete;
    FixedBuffer &operator=(const FixedBuffer &) = delete;
    FixedBuffer(): cur_(data_){}
public:
    int avail() const { return static_cast<int>(end() - cur_); }
    void append(const char* buf, size_t len)
    {
        if (static_cast<size_t>(avail()) > len) //当前可用的空间大于len，则就可以将其添加进去
        {
            memcpy(cur_, buf, len);
            cur_ += len;
        } else if (static_cast<size_t>(avail()) > 0) // 如果小于，只要缓冲区不等于0，则将部分大小放入缓冲区，但是最后一位不能占，因为可能会放结束符'\0'
        { 
            memcpy(cur_, buf, static_cast<size_t>(avail()) - 1);
            cur_ += (static_cast<size_t>(avail()) - 1);
        }
    }

    char *current() { return cur_; }
    const char *data() const { return data_; }
    int length() const { return static_cast<int>(cur_ - data_); }
    void add(size_t len) { cur_ += len; }
    void reset() { cur_ = data_; }                 //重置，只需要把指针移到开头，不需要清零
    void bzero() { ::bzero(data_, sizeof(data_)); } //清零
};


class logStream
{
private:
    typedef logStream self;
public:
    typedef FixedBuffer<kSmallBuffer> Buffer;
    logStream(){}
    logStream(const logStream &) = delete;
    logStream &operator=(const logStream &) = delete;

// 一堆重载，用于流式输出。但是不推荐在多线程真的用流式输出，因为不安全
public:
    // bool类型是真存1假存0，因为无需转换，直接内联
    self &operator<<(bool v) 
    {
        buffer_.append(v ? "1" : "0", 1);
        return *this;
    }

    // 用于输出指针
    self &operator<<(const void *);

    // 一揽子数值类型
    self &operator<<(int);
    self &operator<<(unsigned int);
    self &operator<<(long);
    self &operator<<(unsigned long);
    self &operator<<(long long);
    self &operator<<(unsigned long long);
    self &operator<<(double);

    // 更短的数值型可以用更长的数值型代替
    self &operator<<(float v)
    {
        *this << static_cast<double>(v);
        return *this;
    }

    self& operator<<(short v)
    {
    *this << static_cast<int>(v);
    return *this;
    }

    self& operator<<(unsigned short v)
    {
    *this << static_cast<unsigned int>(v);
    return *this;
    }

    // 字符型直接输出，内联
    self &operator<<(char v)
    {
        buffer_.append(&v, 1);
        return *this;
    }

    self &operator<<(const char *str)
    {
        if (str)
        {
            buffer_.append(str, strlen(str));
        }
        else
        {
            buffer_.append("(null)", 6);
        }
        return *this;
    }

    self &operator<<(const std::string &v)
    {
        buffer_.append(v.c_str(), v.size());
        return *this;
    }
public:
    // 可以直接调用append添加内容
    void append(const char *data, int len) { buffer_.append(data, len); }
    const Buffer& buffer() const { return buffer_; }
    void resetBuffer() { buffer_.reset(); }
private:
    // 成员模板，用于将数字转换为char[]
    template <typename T>
    void formatInteger(T);

    Buffer buffer_;
    // 最大的数值型变量不会超过8个字节即32位
    static const int kMaxNumericSize = 32;
};
