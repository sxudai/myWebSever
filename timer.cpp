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

TimerNode::TimerNode(SP_ReqData _request_data, int timeout):
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
    SP_ReqData req = request_data.lock();
    if(req) {
        req->seperateTimer();
        printf("TimerNode::~TimerNode(), request_data seperateTimer\n");
        if(req->check_checkstate() == CHECK_STATE_INIT) {
            printf("TimerNode::~TimerNode(), request_data timeout, disconnect\n");
            req->disconnect();
        }
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
    printf("func clearReq, timer this ptr: %p", this);
    request_data.reset();
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
        SP_TimerNode cur = TimerNodeQueue.top(); TimerNodeQueue.pop();
        printf("func TimerManager::~TimerManager(), timer ptr: %p", cur.get());
    }
}

void TimerManager::addTimer(SP_ReqData request_data, int timeout){
    SP_TimerNode new_timer = std::make_shared<TimerNode>(request_data, timeout);
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
        printf("TimerManager::handle_expired_event(), top point is %p. state:\n", TimerNodeQueue.top().get());
        printf("TimerNodeQueue.top()->isDeleted(): %d, TimerNodeQueue.top()->isvalid(): %d\n", TimerNodeQueue.top()->isDeleted(), TimerNodeQueue.top()->isvalid());
        if(TimerNodeQueue.top()->isDeleted() || !TimerNodeQueue.top()->isvalid()){
            SP_TimerNode cur = TimerNodeQueue.top(); TimerNodeQueue.pop();
            printf("func TimerManager::handle_expired_event(), timer ptr: %p", cur.get());
        } else {
            break;
        }
    }
}