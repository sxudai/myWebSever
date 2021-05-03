#pragma once

#include <mutex>
#include <queue>
#include <utility>
#include <iostream>


namespace dsx{
  template <typename T>
  class SafeQueue {
  private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
  public:
    SafeQueue() {

    }

    SafeQueue(SafeQueue& other) {
      //TODO:
    }

    ~SafeQueue() {

    }


    bool empty() {
      std::unique_lock<std::mutex> lock(m_mutex);
      return m_queue.empty();
    }
    
    int size() {
      std::unique_lock<std::mutex> lock(m_mutex);
      return m_queue.size();
    }

    void push(T&& t) {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_queue.push(std::forward<T>(t));
    }

    // t传出参数，通过加锁确实实现了线程安全，但是存在类似于假唤醒问题
    // 需要同时完成判断是否传出（返回值），返回参数(传出参数)
    bool pop(T & t){
      std::unique_lock<std::mutex> lock(m_mutex);
      if(m_queue.empty()){
        return false;
      } else{
        t = std::move(m_queue.front());
        m_queue.pop();
      }
      return true;
    }
  };
}
