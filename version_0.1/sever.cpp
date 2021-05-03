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

#include "threadPool.hpp"

#define SERV_PORT 8000
#define OPEN_MAX 512
#define THREADNUM 5
#define BUFLEN 4096

using std::cout;
using std::endl;

// 由于多线程，epoll使用到的东西需要设置为全局变量
// 但是无需加锁，可以保证不会同时访问
int efd;
struct epoll_event temp;
//todo
//保证心跳检查
//struct epoll_event eps[OPEN_MAX];  //用于存从内核得到的事件集合



// 用于创建客户端连接的变量，设置全局变量是不反复创建
struct sockaddr_in cliaddr;
socklen_t clilen = sizeof(cliaddr);

// 当前连接的总客户端数目
int num = 0;

// lfd, 放外边方便用
// 是不是会有线程安全问题，一个没连接好，第二个连接请求又来了怎么办.
// 似乎不会，我们对lfd采用电平触发监听
// 似乎有关epoll底层
int lfd;

// 连接建立事件
int acceptLink(){
    cout << "accept link pid:" << std::this_thread::get_id() << endl;
    int cfd = accept(lfd, (struct sockaddr *)&cliaddr, &clilen);

    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);
    
    // printf("received from %s at PORT %d\n", 
    //         inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)), 
    //         ntohs(cliaddr.sin_port));
    printf("cfd %d---client %d\n", cfd, ++num);

    // 新cfd挂上树
    temp.events = EPOLLIN | EPOLLET; temp.data.fd = cfd;
    int res = epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &temp);

    return 0;
}

// 获取一行 \r\n 结尾的数据 
int get_line(int cfd, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size-1) && (c != '\n')) {  
        n = recv(cfd, &c, 1, 0);
        if (n > 0) {     
            if (c == '\r') {            
                n = recv(cfd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {              
                    recv(cfd, &c, 1, 0);
                } else {                       
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } else {      
            c = '\n';
        }
    }
    buf[i] = '\0';
    
    if (-1 == n)
    	i = n;

    return i;
}


// 客户端端的fd, 错误号，错误描述，回发文件类型， 文件长度 
void send_respond(int cfd, int no, const char *disp, const char *type, int len)
{
	char buf[4096] = {0};
	
	sprintf(buf, "HTTP/1.1 %d %s\r\n", no, disp);
	send(cfd, buf, strlen(buf), 0);
	
	sprintf(buf, "Content-Type: %s\r\n", type);
	sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len);
	send(cfd, buf, strlen(buf), 0);
	
	send(cfd, "\r\n", 2, 0);
}

// 发送服务器本地文件 给浏览器
void send_file(int cfd, const char *file)
{
	int n = 0, ret;
	char buf[4096] = {0};
	
	// 打开的服务器本地文件。  --- cfd 能访问客户端的 socket
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		// 404 错误页面
		perror("open error");
		exit(1);
	}
	
	while ((n = read(fd, buf, sizeof(buf))) > 0) {		
		ret = send(cfd, buf, n, 0);
		if (ret == -1) {
			perror("send error");	
			exit(1);
		}
		if (ret < 4096)		
			printf("-----send ret: %d\n", ret);
	}
	
	close(fd);		
}

// 处理http请求， 判断文件是否存在， 回发
void http_request(int cfd, const char *file)
{
	struct stat sbuf;
	
	// 判断文件是否存在
	int ret = stat(file, &sbuf);
	if (ret != 0) {
		// 回发浏览器 404 错误页面
		perror("stat");
		exit(1);	
	}
	
	if(S_ISREG(sbuf.st_mode)) {		// 是一个普通文件
		
		// 回发 http协议应答
		//send_respond(cfd, 200, "OK", " Content-Type: text/plain; charset=iso-8859-1", sbuf.st_size);	 
		send_respond(cfd, 200, "OK", "Content-Type:image/jpeg", -1);
		//send_respond(cfd, 200, "OK", "audio/mpeg", -1);
		
		// 回发 给客户端请求数据内容。
		send_file(cfd, file);
	}	
}

// 普通epollIn事件
int epollIn(int cfd){
    cout << "pid:" << std::this_thread::get_id() << endl;

    // 检测用返回值
    int res;

    // 读取一行http协议， 拆分， 获取 get 文件名 协议号	
	char line[1024] = {0};
	char method[16], path[256], protocol[16]; 
	
	int len = get_line(cfd, line, sizeof(line)); //读 http请求协议首行 GET /hello.c HTTP/1.1

    if(len == 0){  // cfd连接关闭
        res = epoll_ctl(efd, EPOLL_CTL_DEL, cfd, NULL);
        close(cfd);
        printf("client[%d] closed connection\n", cfd);
    } else{
		sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);	
		printf("method=%s, path=%s, protocol=%s\n", method, path, protocol);
		while (1) {
			char buf[1024] = {0};
			len = get_line(cfd, buf, sizeof(buf));	
			if (buf[0] == '\n') {
				break;	
			} else if (len == -1)
				break;
		}	
	}

    if (strncasecmp(method, "GET", 3) == 0){
		
		http_request(cfd, "./a.jpeg");

        //http协议，交流完一次后，断开连接
        res = epoll_ctl(efd, EPOLL_CTL_DEL, cfd, NULL);
        close(cfd);
        printf("client[%d] closed connection\n", cfd);
		
	}
    return 0;
}

int main(int argc, char *argv[]){
    int threadNum = 3;
    if(argc < 2){
        cout << "use default thread num:"<< threadNum << endl;;
    } else {
        threadNum = atoi(argv[1]);
        cout << "thread num is: " << threadNum << endl;        
    }
     
    //初始化一个线程池
    threadPool pool(threadNum);

    // res用于检查返回值
    int res;

    /*
    lfd设置，即服务器端口设置
    */
    struct sockaddr_in serv_addr; 

    // 创建监听fd
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    // 设置servaddr；
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
    // 设定连接上限
    listen(lfd, 20);

    /*
    epoll设置
    */
   
    // 创建epoll句柄
    efd = epoll_create(OPEN_MAX + 1);
    if(efd == -1) cout << "epoll create error" << endl;
    // 把lfd挂上树
    temp.events = EPOLLIN | EPOLLET; temp.data.fd = lfd;
    res = epoll_ctl(efd, EPOLL_CTL_ADD, lfd, &temp);
    if(res == -1) perror("epoll_ctl error");

    struct epoll_event eps[OPEN_MAX];

    // 开始监听
    int nready;
    while(true){
        // 等待所监控文件描述符上有事件的产生
        // cout << "waiting" << endl;
        nready = epoll_wait(efd, eps, OPEN_MAX, -1);
        // cout << "epoll wait return, nready is : " << nready << endl;

        for(int i=0; i<nready; ++i){
            // 暂时只监听读事件，虽然不会出现写事件，但是以防万一异常
            if(!(eps[i].events & EPOLLIN)) continue;
            // cout << "epoll in" << endl;
            // cout << "cfd: " << eps[i].data.fd << endl;

            // todo:如果这里的fd存的是函数指针的话，似乎可以直接提交完事
            if(eps[i].data.fd == lfd){
                std::future<int> ret = pool.submit(acceptLink);
            } else{
                cout << "read event, cfd: " << eps[i].data.fd << endl;
                std::future<int> ret = pool.submit(epollIn, int(eps[i].data.fd));
            }
        }
    }

    close(lfd);
    close(efd);

    return 0;
}