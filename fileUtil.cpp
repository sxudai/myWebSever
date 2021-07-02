#include <sys/types.h>
#include <string>

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fileUtil.h"

appendFile::appendFile(std::string filename): fp_(::fopen(filename.c_str(), "ae")), writtenBytes_(0) {
    setbuffer(fp_, buffer_, sizeof buffer_);
}

appendFile::~appendFile(){
    fclose(fp_);
}

void appendFile::append(const char *logline, const size_t len){
    size_t n = write(logline, len); //调用内部成员函数，写入
    size_t remain = len - n;
    while (remain > 0) //一个循环，如果没有写完，就继续写
    {
        size_t x = write(logline + n, remain);
        if (x == 0) break;
        n += x;
        remain = len - n; // remain -= x
    }
    writtenBytes_ += len;
}

size_t appendFile::write(const char* logline, size_t len) {
  return fwrite_unlocked(logline, 1, len, fp_);
}