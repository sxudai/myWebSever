#pragma once

#include <sys/types.h>
#include <string>

// not thread safe
class appendFile
{
public:
    explicit appendFile(std::string filename);
    appendFile &operator=(appendFile &) = delete;
    appendFile(const appendFile &) = delete;

    ~appendFile();

    void append(const char *logline, const size_t len);

    void flush() { fflush(fp_); }

    size_t writtenBytes() const { return writtenBytes_; } //已经写入文件的大小

private:
    size_t write(const char *logline, size_t len);

    FILE *fp_;               //文件指针的缓冲区指针
    char buffer_[64 * 1024]; //默认是一个这么大，fflush有两种前提，一个是缓冲区满，一个是直接调用fflush
    size_t writtenBytes_;  // 用于记录写了多少字节，配合后边文件滚动
};