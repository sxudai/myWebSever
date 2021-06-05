#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <ctype.h>
#include<string.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>

#include "util.h"
#include "timer.h"
#include "request.h"
#include "threadPool.hpp"

#define SERV_PORT 8000
#define OPEN_MAX 65535
#define THREADNUM 5
#define BUFLEN 4096

using std::cout;
using std::endl;

uniRequest* uniRequestLfd;
TimerManager timerManag;

int handle_request(void *inputRequest){
    printf("handle_request \n");
    int res = 1;
    uniRequest * request = static_cast<uniRequest*>(inputRequest);
    cout << "lfd: " << request->get_lfd() << "cfd: " << request->get_cfd() << endl;
    if(request->get_lfd() == request->get_cfd()){
        res = request->acceptLink();
    } else {
        // 先分离timer
        request->seperateTimer();
        res = request->epollIn();
    }
    return res;
}

int init_lfd(int efd){
    //用于接收返回值
    int res;
     
    // 创建监听fd
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    setSocketNonBlocking(lfd);

    // 设置servaddr；
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_PORT);

    // 端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定端口
    res = bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if(res < 0) cout << "bind error" << endl;
    // listen，设定连接上限
    listen(lfd, OPEN_MAX);

    // 把lfd挂上树
    struct epoll_event temp;
    temp.events = EPOLLIN | EPOLLET;
    // todo 正确销毁了吗？
    //应该不能换成共享指针
    uniRequestLfd = new uniRequest(lfd, lfd, efd);
    temp.data.ptr = static_cast<void *>(uniRequestLfd);
    res = epoll_ctl(efd, EPOLL_CTL_ADD, lfd, &temp);
    if(res == -1) perror("lfd epoll_ctl add error");

    return lfd;
}

int main(int argc, char *argv[]){
    //修改sigpipe信号处理方式
    handle_for_sigpipe(SIGPIPE);

    //初始化一个线程池
    threadPool pool(3);

    // 创建epoll句柄
    int efd = epoll_create(OPEN_MAX + 1);
    if(efd == -1) cout << "epoll create error" << endl;
    struct epoll_event eps[OPEN_MAX];

    // 创建lfd
    int lfd = init_lfd(efd);
    if(lfd == -1) cout << "lfd init error" << endl;

    // cout << "lfd: " << lfd << endl;

    while(true){
        // 等待所监控文件描述符上有事件的产生
        // cout << "waiting" << endl;
        int nready = epoll_wait(efd, eps, OPEN_MAX, -1);
        // cout << "epoll wait return, nready is : " << nready << endl;

        for(int i=0; i<nready; ++i){
            // 暂时只监听读事件，虽然不会出现写事件，但是以防万一异常
            if(!(eps[i].events & EPOLLIN)) continue;
            uniRequest * request = static_cast<uniRequest*>(eps[i].data.ptr);
            printf("sever, set_CHECK_STATE_REQUESTLINE, req point is %p\n", request);
            request->set_CHECK_STATE_REQUESTLINE();
            printf("submit job to thread pool\n");
            std::future<int> ret = pool.submit(handle_request, static_cast<void *>(eps[i].data.ptr));
            printf("job submited\n");
        }
        // std::this_thread::sleep_for(std::chrono::seconds(2));
        printf("main, start to handle_expired_event()\n");
        timerManag.handle_expired_event();
        printf("main, ebd of handle_expired_event()\n");
    }
    
    close(lfd);
    close(efd);

    printf("main, free\n");
    delete uniRequestLfd;

    return 0;
}