#include <thread>
#include <condition_variable>
#include <mutex>
#include <list>
#include <queue>
#include <future>
#include <functional>
#include <chrono>
#include <atomic>
#include <algorithm>

#include "SafeQueue.hpp"

#define threadManageStep 2
#define maxThreadNum 20
#define minThreadNum 3
#define MAXTASK 1024

class threadPool {
private:
    struct worker{
    private:
        threadPool* m_pool;
        std::thread m_worker;
        bool m_stop;
    public:
        worker(threadPool* pool_):m_pool(pool_), m_stop(false){
            m_worker = std::move(std::thread([this]() {
                while(true) {
                    std::function<void()> task;
                    bool isTask = false;
                    // 这里要用{}括起来的原因是，将这一块作为一个局部作用域, 当走出这个作用域，局部变量ul析构，会自动解锁
                    {
                        std::unique_lock<std::mutex> ul(m_pool->mtx_);
                        // cv.wait(ul, pred)；只有当pred为false时，线程才会wait；被唤醒时，只有当pred为true时才会运行
                        // 进入时，thread2Destory > 0时，有线程要销毁，不必阻塞；pool->stop_=true时线程池已gg不必阻塞；任务队列非空时不需要阻塞
                        (m_pool->cv_).wait(ul, [this](){return m_pool->stop_ || !(m_pool->tasks_).empty() || m_pool->thread2Destory > 0;});
                        // 前一种情况是管理线程要销毁线程，后一种情况是线程池自己完蛋了。后一种情况用两个判断条件主要是为了就算线程池终止了，也把剩下的工作做完
                        if(m_pool->thread2Destory > 0 || (m_pool->stop_ && m_pool->tasks_.empty())) {
                            // 先检查下thread2Destory，防止类似假唤醒问题。
                            // 如果没有要销毁线程，进程池也没有终止，说明是要销毁线程，但其他线程已经销毁。此时不用再销毁了
                            // 这里直接continue不用担心是被submit唤醒，被submit唤醒不会进入if
                            if(m_pool->thread2Destory == 0 && !(m_pool->stop_)) continue;
                            --(m_pool->thread2Destory);
                            --(m_pool->liveThread);
                            // std::cout << "thread go to destory" << std::endl;
                            m_stop = true;
                            return; 
                        }
                        isTask = m_pool->tasks_.pop(task);
                        // std::cout << "work thread: " << std::this_thread::get_id() << std::endl;
                    }
                    ++(m_pool->busyThread);
                    if(isTask) task();
                    --(m_pool->busyThread);
                }
            }));
        }

        void join(){m_worker.join();}

        bool joinable(){return m_stop;}

    };
public:
    // 先初始化线程池，再启动初始化线程
    explicit threadPool(size_t threadNum): stop_(false), thread2Destory(0), liveThread(0), busyThread(0) {
        for(size_t i = 0; i < threadNum; ++i) {
            workers_.emplace_back(this);
            ++liveThread;
        }

        manager = std::move(std::thread([this](){
            while(!stop_){
                // 定期检查
                if(tasks_.size() > 0) cv_.notify_all();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                // 具体创建多少，销毁多少，要不要销毁交给函数去决定
                std::cout << "manaer is checking and task nums is: " << tasks_.size() << std::endl;
                getState();
                if(busyThread * 2 < liveThread){
                    workerDestory();
                } else if(tasks_.size() > liveThread){
                    workerCreate();
                }
            }
        }));

    }

    void workerCreate(){
        // 总线程数小于最大线程数，创建线程数小于threadManageStep
        for(int j = 0; liveThread < maxThreadNum && j < threadManageStep; ++liveThread, ++j){
            // std::cout << "creating new thread: " << j << std::endl;
            {
                std::unique_lock<std::mutex> ul(mtx_);
                workers_.emplace_back(this);
            }
            // ++liveThread;
        }
    }

    // 唤醒线程，他们会自己结束，该函数负责回收线程资源
    void workerDestory(){
        if(liveThread <= minThreadNum) return;
        thread2Destory = std::min(threadManageStep, liveThread-minThreadNum);
        cv_.notify_all();
        int i = thread2Destory;
        auto iter = workers_.begin();
        while(liveThread >= minThreadNum && !workers_.empty() && i > 0){
            if(iter == workers_.end()){
                iter = workers_.begin();
            } else if((*iter).joinable()){
                (*iter).join();
                {
                    std::unique_lock<std::mutex> ul(mtx_);
                    iter = workers_.erase(iter);
                }
                --i;
                // std::cout << "remain thread to joinning: " << i << std::endl;
            } else {
                ++iter;
            }
        }
    }

    ~threadPool() {
        stop_ = true;
        cv_.notify_all();
        manager.join();
        std::cout << "manager joined" << std::endl;
        for(auto& worker : workers_) {
            worker.join();
        }
        std::cout << "workers joined" << std::endl;
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        // 函数与参数绑定，function包装准备执行
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        // packaged_task包装，异步返回结果；shared_ptr: packaged_task copy ctor=delete
        // task_ptr类型：shared_ptr<std::packaged_task<decltype(f(args...))()>>
        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        // function再包装，统一无返回值,方便放入队列
        std::function<void()> wrapper_func = [task_ptr]() {
            (*task_ptr)(); 
        };

        // 任务入队
        // 使用线程安全队列，这样无需线程池自己加锁
        
        tasks_.push(std::move(wrapper_func));
        

        // 唤醒
        cv_.notify_one();

        // 异步返回
        return task_ptr->get_future();
    }

    void getState(){
        std::cout << "pool state" << std::endl;
        std::cout << "liveThread: " << liveThread << std::endl;
        std::cout << "busyThread: " << busyThread << std::endl;
    }

    bool isShutDown(){
        std::cout << manager.joinable() << std::endl;
        return stop_;
    }

private:
    std::atomic<bool> stop_;
    std::atomic<int> liveThread;
    std::atomic<int> busyThread;
    std::atomic<int> thread2Destory;
    std::list<worker> workers_;         // 线程集合
    dsx::SafeQueue<std::function<void()>> tasks_;  // 任务队列
    std::mutex mtx_;                           // 用于锁住整个线程池，一次只能有一个线程涉及更改线程池的操作
    std::condition_variable cv_;               // 用于检测任务队列是否为空

    std::thread manager;
};
