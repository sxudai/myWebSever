#include "logStream.h"

const char digits[] = "9876543210123456789";
const char* zero = digits + 9;
const char digitsHex[] = "0123456789ABCDEF";

// 高效转化数字->字符串
template <typename T>
size_t convert(char buf[], T value) {
  T i = value;
  char* p = buf;

  do {
    int lsd = static_cast<int>(i % 10);
    i /= 10;
    *p++ = zero[lsd];
  } while (i != 0);

  if (value < 0) {
    *p++ = '-';
  }
  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

// 用于输出指针
size_t convertHex(char buf[], uintptr_t value)
{
  uintptr_t i = value;
  char* p = buf;

  do
  {
    int lsd = static_cast<int>(i % 16);
    i /= 16;
    *p++ = digitsHex[lsd];
  } while (i != 0);

  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

// 显示实例化
template class FixedBuffer<kSmallBuffer>;
template class FixedBuffer<kLargeBuffer>;

template <typename T>
void logStream::formatInteger(T v) {
    // buffer容不下kMaxNumericSize个字符的话会被直接丢弃
    if (buffer_.avail() >= kMaxNumericSize) {
        size_t len = convert(buffer_.current(), v);
        buffer_.add(len);
    }
}

logStream& logStream::operator<<(int v)
{
    formatInteger(v);
    return *this;
}

logStream& logStream::operator<<(unsigned int v)
{
    formatInteger(v);
    return *this;
}

logStream& logStream::operator<<(long v)
{
    formatInteger(v);
    return *this;
}

logStream& logStream::operator<<(unsigned long v)
{
    formatInteger(v);
    return *this;
}

logStream& logStream::operator<<(long long v)
{
    formatInteger(v);
    return *this;
}

logStream& logStream::operator<<(unsigned long long v)
{
    formatInteger(v);
    return *this;
}

logStream& logStream::operator<<(const void* p)
{
    // typedef unsigned long int	uintptr_t;
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    if (buffer_.avail() >= kMaxNumericSize)
    {
        char* buf = buffer_.current();
        buf[0] = '0';
        buf[1] = 'x';
        size_t len = convertHex(buf+2, v);
        buffer_.add(len+2);
    }
    return *this;
}

logStream& logStream::operator<<(double v)
{
    if (buffer_.avail() >= kMaxNumericSize)
    {
        int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", v);
        buffer_.add(len);
    }
    return *this;
}