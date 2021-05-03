#include "threadPool.hpp"
#include <iostream>
#include <future>
#include <thread>
#include <chrono>

using std::cout;
using std::endl;

int fun(int a, int b){
    std::cout << "sleeping" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));
    int res = a * b;
    return res;
}

int main(){
    threadPool pool(3);
    std::vector<std::future<int>> results;
    for(int i = 0; i < 10; ++i) {
        // std::this_thread::sleep_for(std::chrono::seconds(1));
        results.emplace_back(pool.submit(fun, i, i+1));
        // pool.getState();
    }
    // pool.getState();
    for(auto & result: results)
        std::cout << result.get() << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(20));
    pool.getState();
    cout << pool.isShutDown() << endl;;

    return 0;
}


