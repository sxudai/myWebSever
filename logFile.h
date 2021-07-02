#pragma once

#include <memory>
#include <string>
#include <mutex>

#include "fileUtil.h"

class appendFile;

class logFile
{ //logFile进一步封装了FileUtil，并设置了一个循环次数，没过这么多次就flush一次。
public:
    logFile(const std::string &basename,
            size_t rollSize,
            int flushInterval = 3,
            int checkEveryN = 1024);

    void append(const char *logline, int len); //添加
    void flush();                              //刷新缓冲好区
    bool rollFile();                           //滚动日志

private:
    void append_unlocked(const char *logline, int len); //不加锁的方案添加

    static std::string getlogFileName(const std::string &basename, time_t *now);  // 为新日志生成一个文件名

    const std::string basename_; //日志文件 basename
    const size_t rollSize_;      //日志文件达到了一个rollsize换一个新文件
    const int flushInterval_;    //日志写入间隔时间，日志是间隔一段时间才写入，不然开销太大
    const int checkEveryN_;

    int count_;

    std::mutex mutex_;
    time_t startOfPeriod_;                       //开始记录日志时间（将会调整到0点的时间）
    time_t lastRoll_;                            //上一次滚动日志文件的时间
    time_t lastFlush_;                           //上一次日志写入文件的时间
    std::unique_ptr<appendFile> file_;

    const static int kRollPerSeconds_ = 60 * 60 * 24; // 24小时
};