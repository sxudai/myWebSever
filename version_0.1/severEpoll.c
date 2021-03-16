#include <stdio.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <ctype.h>
#include <poll.h>
#include <sys/epoll.h>

#define MAX_EVENTS  1024                                    //监听上限数
#define BUFLEN 4096
#define SERV_PORT   8080

void recvdata(int fd, int events, void *arg);
void senddata(int fd, int events, void *arg);

struct myevent_s {
    int fd;                                                 //要监听的文件描述符
    int events;                                             //对应的监听事件
    void *arg;                                              //泛型参数
    void (*call_back)(int fd, int events, void *arg);       //回调函数
    int status;                                             //是否在监听:1->在红黑树上(监听), 0->不在(不监听)
    char buf[BUFLEN];
    int len;
    long last_active;                                       //记录每次加入红黑树 g_efd 的时间值
};

int g_epfd;
struct myevent_s g_events[MAX_EVENTS+1];

void eventset(struct myevent_s * ev, int fd,  void (*call_back)(int fd, int events, void *arg),  void *arg )
{
    ev->fd = fd;
    ev->events = 0;
    ev->call_back = call_back;
    ev->arg = arg;
    ev->status = 0;
    ev->last_active = time(NULL);

    return ;
}

void eventadd(int efd, int events, struct myevent_s *ev)
{
    struct epoll_event epv;
    int op;
    epv.events = ev->events = events;
    epv.data.ptr = ev;
    printf("state: %d", ev->status == 1);

    if(ev->status == 1){
        op = EPOLL_CTL_MOD;
    } else {
        op = EPOLL_CTL_ADD;
        ev->status = 1;
    }

    if (epoll_ctl(efd, op, ev->fd, &epv) < 0)                       //实际添加/修改
        printf("event add failed [fd=%d], events[%d]\n", ev->fd, events);
    else
        printf("event add OK [fd=%d], op=%d, events[%0X]\n", ev->fd, op, events);

    return ;

}

void eventdel(int efd, struct myevent_s *ev)
{
    struct epoll_event epv = {0, {0}};
    if (ev->status != 1)
        return ;
    // epv.events = ev->events;
    epv.data.ptr = ev;
    epoll_ctl(efd, EPOLL_CTL_DEL, ev->fd, &epv);
    ev->status = 0;
}

void recvdata(int fd, int events, void *arg)
{
    struct myevent_s *ev = (struct myevent_s *)arg;
    int len;

    len = recv(fd, ev->buf, sizeof(ev->buf), 0);
    eventdel(g_epfd, ev);
    if (len>0){
        ev->len = len;
        ev->buf[len] = '\0';
        printf("%d\n", len);
        printf("C[%d]:%s\n", fd, ev->buf);
        eventset(ev, fd, senddata, ev);
        eventadd(g_epfd, EPOLLOUT, ev);
        printf("recv end\n");
    } else {
        close(ev->fd);
    }
    return ;
}

void senddata(int fd, int events, void *arg)
{
    struct myevent_s *ev = (struct myevent_s *)arg;
    int len;
    printf("send stage ev->len: %d\n", ev->len);

    len = send(fd, ev->buf, ev->len, 0);

    if (len>0){
        eventdel(g_epfd, ev);
        eventset(ev, fd, recvdata, ev);
        eventadd(g_epfd, EPOLLIN, ev);
    } else {
        printf("hereh?\n");
        close(ev->fd);
        eventdel(g_epfd, ev);
    }
    return ;
}

void acceptcoon(int lfd, int events, void* arg)
{
    struct sockaddr_in cin;
    socklen_t len = sizeof(cin);
    int cfd, i;
    cfd = accept(lfd, (struct sockaddr *)&cin, &len);
    for (i = 0; i < MAX_EVENTS; i++){
        if (g_events[i].status == 0){
            break;
        }
    }
    fcntl(cfd, F_SETFL, O_NONBLOCK);

    eventset(&g_events[i], cfd, recvdata, &g_events[i]);
    eventadd(g_epfd, EPOLLIN, &g_events[i]);
}

void initlistensocket(int epfd, short port)
{
  int lfd;
  lfd = socket(AF_INET,SOCK_STREAM, 0);
  fcntl(lfd, F_SETFL, O_NONBLOCK);
  struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	bind(lfd, (struct sockaddr *)&sin, sizeof(sin));
	listen(lfd, 20);

  eventset(&g_events[MAX_EVENTS], lfd, acceptcoon, &g_events[MAX_EVENTS]);
  eventadd(g_epfd, POLLIN, &g_events[MAX_EVENTS]);

    return ;
}

int main(int argc, char *argv[])
{
    int g_epfd = epoll_create(MAX_EVENTS+1);
    initlistensocket(g_epfd, SERV_PORT);

    struct epoll_event events[MAX_EVENTS+1];

    int i;

    while(1){
        int nfd = epoll_wait(g_epfd, events, MAX_EVENTS+1, 1000);

        for (i = 0; i < nfd; i++) {
            struct myevent_s *ev = (struct myevent_s *)events[i].data.ptr;
            // printf("exeing ------> [fd=%d], events[%0X]\n", ev->fd, ev->events);
            ev->call_back(ev->fd, events[i].events, ev->arg);
        }
    }
    return 0;
}
