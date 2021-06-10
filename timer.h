#pragma once
#include <unistd.h>
#include <memory>
#include <queue>
#include <deque>
#include <mutex>

#include "dsx.h"
#include "request.h"

class uniRequest;

class TimerNode{
public:
    typedef std::shared_ptr<uniRequest> SP_ReqData;
    typedef std::weak_ptr<uniRequest> WP_ReqData;
private:
    bool deleted;
    size_t expired_time;
    WP_ReqData request_data;
public:
    TimerNode(SP_ReqData _request_data, int timeout);
    ~TimerNode();
    void update(int timeout);
    bool isvalid();
    void clearReq();
    void setDeleted();
    bool isDeleted() const;
    size_t getExpTime() const;
};

struct timerCmp{
    bool operator()(const std::shared_ptr<TimerNode> a, const std::shared_ptr<TimerNode> b) const
    {
        return a->getExpTime() > b->getExpTime();
    }
};

class TimerManager{
public:
    typedef std::shared_ptr<TimerNode> SP_TimerNode;
    typedef std::shared_ptr<uniRequest> SP_ReqData;
private:
    // 用优先队列可以接受，log的复杂度还行
    // 用deque做底层的原因：要在频繁在头部删除节点，deque性能更好
    std::priority_queue<SP_TimerNode, std::deque<SP_TimerNode>, timerCmp> TimerNodeQueue;
    std::mutex mtx;
public:
    TimerManager();
    ~TimerManager();
    void addTimer(SP_ReqData request_data, int timeout);
    void handle_expired_event();
};