#pragma once

#include <string>

#include "logStream.h"

class logger{
private:
    class impl{
    public:
        impl(const char *fileName, int line);
        void formatTime();      //格式化日期
        void finish();          //将日志写到缓冲区

        logStream stream_;      //LogStream类对象成员
        int line_;              //行号
        std::string basename_;  // 文件名
    };
private:
    impl impl_;
    // std::string filename;       // 文件名
public:
    logger(const char *fileName, int line);
    ~logger();

    logStream& stream(){return impl_.stream_;}

public:
    typedef void (*OutputFunc)(const char *msg, int len);
    typedef void (*FlushFunc)();

    static void setOutput(OutputFunc); //设置输出函数
    static OutputFunc g_output;
};

// 这个后边单独放到我的头文件里是不是比较好？
#define LOG logger(__FILE__, __LINE__).stream()