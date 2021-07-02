#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <condition_variable>

#include "logStream.h"

using std::string;

class asyncLogging{
private:
    // 主函数
    void threadFunc();
    
    typedef FixedBuffer<kLargeBuffer> Buffer; //缓冲区类型
    typedef std::vector<std::unique_ptr<Buffer>> BufferVector; //缓冲区列表类型
    typedef BufferVector::value_type BufferPtr;    // BufferVector::value_type = std::unique_ptr<Buffer>

    const int flushInterval_; // 超时时间,在flushInterval_秒没,缓冲区没写满,仍将缓冲区内的数据写到文件中
    std::atomic<bool> running_;
    const string basename_;   // 日志文件的基础名字
    const size_t rollSize_;   // 日志文件的滚动大小
    
    std::mutex mutex_;
    std::condition_variable cond_;
    BufferPtr currentBuffer_; //当前缓冲区
    BufferPtr nextBuffer_;   //预备缓冲区
    BufferVector buffers_;    //将写入文件的已填满的缓冲区
    std::thread thread_;  // 工作线程

public:
    asyncLogging(asyncLogging & ) = delete;
    asyncLogging &operator=(asyncLogging &) = delete;
    asyncLogging(const string& basename,
               size_t rollSize,
               int flushInterval = 3);

    ~asyncLogging()
    {
        if(running_) stop();
    }

    void append(const char* logline, int len);

    void stop()
    {
        running_ = false;
        cond_.notify_all();
        thread_.join();
    }
};