#pragma once
#include <unistd.h>
#include <memory>
#include <queue>
#include <deque>
#include <mutex>

#include "request.h"

class uniRequest;

class TimerNode
{
private:
    bool deleted;
    size_t expired_time;
    uniRequest* request_data;
public:
    TimerNode(uniRequest* _request_data, int timeout);
    ~TimerNode();
    void update(int timeout);
    bool isvalid();
    void clearReq();
    void setDeleted();
    bool isDeleted() const;
    size_t getExpTime() const;
};

struct timerCmp
{
    bool operator()(const TimerNode *a, const TimerNode *b) const
    {
        return a->getExpTime() > b->getExpTime();
    }
};

class TimerManager
{
private:
    // 用优先队列可以接受，log的复杂度还行
    // 用deque做底层的原因：要在频繁在头部删除节点，deque性能更好
    std::priority_queue<TimerNode*, std::deque<TimerNode*>, timerCmp> TimerNodeQueue;
    std::mutex mtx;
public:
    TimerManager();
    ~TimerManager();
    void addTimer(uniRequest * request_data, int timeout);
    void handle_expired_event();
};