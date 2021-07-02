#include <time.h>

#include "logFile.h"

using std::string;

logFile::logFile(const string &basename,
                 size_t rollSize,
                 int flushInterval,
                 int checkEveryN)
    : basename_(basename),
      rollSize_(rollSize),
      flushInterval_(flushInterval),
      checkEveryN_(checkEveryN),
      count_(0),
      startOfPeriod_(0),
      lastRoll_(0),
      lastFlush_(0)
{
    rollFile();  //滚动一个日志，也就是获取一个文件
}

bool logFile::rollFile(){
    time_t now = 0;
    string filename = getlogFileName(basename_, &now);
    //这里是对其到krollperseconds的整数倍，也就是时间调整到当天的0时
    time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;  // 取整的操作，回归到0点

    if (now > lastRoll_)
    {
        lastRoll_ = now;
        lastFlush_ = now;
        startOfPeriod_ = start;
        file_.reset(new appendFile(filename));
        return true;
    }
    return false;
}

//返回一个log文件名 
string logFile::getlogFileName(const string &basename, time_t *now){ 
    string filename;
    filename = basename;

    char timebuf[32];
    struct tm tm;
    *now = time(NULL);  // 获取系统时间，单位为秒;
    gmtime_r(now, &tm);
    strftime(timebuf, sizeof(timebuf), ".%Y%m%d-%H%M%S", &tm);
    filename += timebuf;

    filename += ".log";

    return filename;
}

void logFile::append(const char *logline, int len)
{
    std::unique_lock<std::mutex> ul(mutex_);

    file_->append(logline, len); //一旦写入文件

    if (file_->writtenBytes() > rollSize_) //就要判断是否需要滚动日志
    {
        rollFile();
    }
    else
    {
        ++count_;
        if (count_ >= checkEveryN_) //第二个需要滚动的条件：时间
        {
            count_ = 0;
            time_t now = ::time(NULL);
            time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_; //调整到0点
            if (thisPeriod_ != startOfPeriod_)                              //如果不等，说明是第二天了，就应该滚动了
            {
                rollFile();
            }
            else if (now - lastFlush_ > flushInterval_) //判断是否应该刷新
            {
                lastFlush_ = now;
                file_->flush();
            }
        }
    }
}

void logFile::flush(){
    std::unique_lock<std::mutex> ul(mutex_);
    file_->flush();
}