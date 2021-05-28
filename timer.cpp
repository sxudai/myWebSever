#include <memory>
#include <queue>
#include <deque>
#include <mutex>
#include <ctime>
#include <iostream>

#include <unistd.h>
#include <sys/time.h>

#include "timer.h"
#include "request.h"

using std::cout;
using std::endl;

TimerNode::TimerNode(uniRequest*_request_data, int timeout):
    request_data(_request_data),
    deleted(false)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    // 以毫秒计
    expired_time = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
    // cout << "curr time: " << ((now.tv_sec * 1000) + (now.tv_usec / 1000)) << "expired_time: " << expired_time << endl;
}
TimerNode::~TimerNode(){
    // timer超时,先断开连接
    // if(request_data){
    //     request_data->disconnect();
    // }
}

//更新超时时间
void TimerNode::update(int timeout){
    struct timeval now;
    gettimeofday(&now, NULL);
    expired_time = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
}

// 判断当前timer是否超时
bool TimerNode::isvalid(){
    struct timeval now;
    gettimeofday(&now, NULL);
    size_t curr = ((now.tv_sec * 1000) + (now.tv_usec / 1000));
    // cout << "func isValid(), curr is: " << curr << ", expire time is: " << expired_time << " isValid: "<< (curr < expired_time) << endl;
    if(curr < expired_time && !deleted) return true;
    return false;
}

// 清除请求，代表request和timer分离
void TimerNode::clearReq(){
    // cout << "func clearReq, timer this ptr: " << this << endl;
    request_data = nullptr;
    deleted = true;  // 毁灭吧
}

void TimerNode::setDeleted(){deleted = true;}
bool TimerNode::isDeleted() const{return deleted;}
size_t TimerNode::getExpTime() const{return expired_time;}

// 没有需要初始化的东西
TimerManager::TimerManager(){}

TimerManager::~TimerManager(){
    //释放节点
    while(!TimerNodeQueue.empty()){
        TimerNode *cur = TimerNodeQueue.top(); TimerNodeQueue.pop();
        cout << "func TimerManager::~TimerManager(), timer ptr: " << cur << endl;
        delete cur;
    }
}

void TimerManager::addTimer(uniRequest * request_data, int timeout){
    TimerNode *new_timer = new TimerNode(request_data, timeout);
    {
        std::lock_guard<std::mutex> ul(mtx);
        TimerNodeQueue.push(new_timer);
    }
    request_data->linkTimer(new_timer);
}
void TimerManager::handle_expired_event(){
    // 理论上来说是不要加锁的，但是以防万一
    std::lock_guard<std::mutex> ul(mtx);
    while(!TimerNodeQueue.empty()){
        if(!TimerNodeQueue.top()->isDeleted() || TimerNodeQueue.top()->isvalid()) break;
        TimerNode *cur = TimerNodeQueue.top(); TimerNodeQueue.pop();
        cout << "func TimerManager::handle_expired_event(), timer ptr: " << cur << endl;
        delete cur;
    }
}