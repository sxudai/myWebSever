// #include <errno.h>
#include <time.h>  
#include <pthread.h>
#include <sys/time.h> 
#include <thread>

#include "logger.h"

void defaultOutput(const char *msg, int len)
{
    size_t n = fwrite(msg, 1, len, stdout);
}

void defaultFlush()
{
    fflush(stdout);
}

logger::OutputFunc logger::g_output = defaultOutput;

void logger::setOutput(OutputFunc out)
{
    g_output = out;  //要修改输出的位置，更改为新的输出，默认是stdout
}

logger::logger(const char *fileName, int line): impl_(fileName, line) {}


logger::~logger(){
    const logStream::Buffer& buf(stream().buffer());
    g_output(buf.data(), buf.length());
}

logger::impl::impl(const char *fileName, int line): basename_(fileName), line_(line), stream_() {
    formatTime();
    stream_ << reinterpret_cast<void *>(pthread_self()) << '\n';
    stream_ << "fileName: " << basename_ << "\nline: " << line_ << '\n';
}

void logger::impl::formatTime()
{
    struct timeval tv;
    time_t time;
    char str_t[26] = {0};
    gettimeofday (&tv, NULL);
    time = tv.tv_sec;
    struct tm* p_time = localtime(&time);   
    strftime(str_t, 26, "%Y-%m-%d %H:%M:%S\n", p_time);
    stream_ << str_t; 
}