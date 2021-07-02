#include <ctime>
#include <chrono>
#include <functional>

#include "logFile.h"
#include "asyncLogging.h"


asyncLogging::asyncLogging(const string& basename,
                           size_t rollSize,
                           int flushInterval)
    :   flushInterval_(flushInterval),
        running_(true),
        basename_(basename),
        rollSize_(rollSize),
        thread_(std::bind(&asyncLogging::threadFunc, this)),  // 可以这么写，类函数都有一个隐藏参数：类指针。&asyncLogging::threadFunc是类成员函数指针
        currentBuffer_(new Buffer()), //首先准备了两块缓冲区,当前缓冲区
        nextBuffer_(new Buffer()),    //预备缓冲区
        buffers_()
{
    currentBuffer_->bzero(); //将缓冲区清零
    nextBuffer_->bzero();
    buffers_.reserve(16); //缓冲区指针列表预留16个空间
}

void asyncLogging::append(const char* logline, int len)
{
    //因为有多个线程要调用append,所以用mutex保护
    std::unique_lock<std::mutex> ul(mutex_);
    if (currentBuffer_->avail() > len)//判断一下当前缓冲区是否满了
    {
        //当缓冲区未满,将数据追加到末尾
        currentBuffer_->append(logline, len);
    }
    else
    {
        //当缓冲区已满,将当前缓冲区添加到待写入文件的以填满的缓冲区
        buffers_.push_back(std::move(currentBuffer_));

        //将当前缓冲区设置为预备缓冲区
        if (nextBuffer_)
        {
            currentBuffer_ = std::move(nextBuffer_);
        }
        else//相当于nextbuffer也没有了
        {
            //这种情况极少发生,前端写入速度太快了,一下把两块缓冲区都写完了，那么之后分配一个新的缓冲区
            currentBuffer_.reset(new Buffer()); 
        }
        currentBuffer_->append(logline, len);
        cond_.notify_all();//通知后端开始写入日志
    }
}

void asyncLogging::threadFunc()
{
    logFile output(basename_, rollSize_, false);
    /*一开始就准备了两块缓冲区
    * 一块用来替换current buffer
    * 一块用来做备用buffer
    */
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->bzero();
    newBuffer2->bzero();
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    while(running_)
    {
        {
            std::unique_lock<std::mutex> ul(mutex_);
            // 处理假唤醒问题
            while(running_ && buffers_.empty()) cond_.wait_for(ul,std::chrono::seconds(flushInterval_));
            // 将当前缓冲区压入buffers，来都来了，也许current buffer里也有东西，顺便把它一起写了
            buffers_.push_back(std::move(currentBuffer_));
            //将空闲的newbuffer1置为当前缓冲区_
            currentBuffer_ = std::move(newBuffer1);
            //buffers与bufferstowrite交换,这样后面的代码可以在临界区之外方位bufferstowrite
            buffersToWrite.swap(buffers_);
            //确保前端始终有一个预留的buffer可供调配, 减少前端临界区分配内存的概率,缩短前端
            if (!nextBuffer_) nextBuffer_ = std::move(newBuffer2);
        }

        /*消息堆积
        * 前端陷入死循环,拼命发送日志消息,超过后端的处理能力,这就是典型的生产速度
        * 超过消费速度的问题,会造成数据在内存中堆积,严重时会引发性能问题,(可用内存不足)
        * 或程序崩溃(分配内存失败)
        */
        if (buffersToWrite.size() > 25)//需要后端写的日志块超过25个,可能是前端日志出现了死循环
        {
            char buf[256];
            time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            snprintf(buf, sizeof(buf), "Dropped log messages at %s, %zd larger buffers\n",
                    ctime(&tt),
                    buffersToWrite.size()-2);//表明是前端出现了死循环
            fputs(buf, stderr);
            output.append(buf, static_cast<int>(strlen(buf)));
            buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end()); //丢掉多余日志,以腾出内存,只保留2块
        }

        // 后端开写
        for (const auto& buffer : buffersToWrite) output.append(buffer->data(), buffer->length());

        // 只保留两个buffer, 用于可能的newbuffer1和newbuffer2
        if (buffersToWrite.size() > 2) buffersToWrite.resize(2);

        // 检查两块缓冲区
        if (!newBuffer1)
        {
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if (!newBuffer2)
        {
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }
        buffersToWrite.clear();
        // 刷入磁盘
        output.flush();
    }
  output.flush();
}