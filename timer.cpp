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
#include "dsx.h"

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
    if(request_data) {
        request_data->seperateTimer();
        printf("TimerNode::~TimerNode(), request_data seperateTimer\n");
    }
    if(request_data) cout << "request_data->check_checkstate(): " << request_data->check_checkstate() << endl;
    if(request_data && request_data->check_checkstate() == CHECK_STATE_INIT){
        request_data->disconnect();
    }
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
    cout << "func clearReq, timer this ptr: " << this << endl;
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
        printf("TimerManager::~TimerManager() free\n");
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
    // 防止一边加timer一边释放timer
    std::lock_guard<std::mutex> ul(mtx);
    while(!TimerNodeQueue.empty()){
        printf("TimerManager::handle_expired_event(), top point is %p. state:\n", TimerNodeQueue.top());
        printf("TimerNodeQueue.top()->isDeleted(): %d, TimerNodeQueue.top()->isvalid(): %d\n", TimerNodeQueue.top()->isDeleted(), TimerNodeQueue.top()->isvalid());
        if(TimerNodeQueue.top()->isDeleted() || !TimerNodeQueue.top()->isvalid()){
            TimerNode *cur = TimerNodeQueue.top(); TimerNodeQueue.pop();
            cout << "func TimerManager::handle_expired_event(), timer ptr: " << cur << endl;
            printf("TimerManager::handle_expired_event() free\n");
            delete cur;
        } else {
            break;
        }

        // if(!TimerNodeQueue.top()->isDeleted() || TimerNodeQueue.top()->isvalid()) break;
        // TimerNode *cur = TimerNodeQueue.top(); TimerNodeQueue.pop();
        // cout << "func TimerManager::handle_expired_event(), timer ptr: " << cur << endl;
        // delete cur;
    }
}