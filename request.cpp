#include "request.h"
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
#include <thread>

#include <iostream>


#include "util.h"

using std::cout;
using std::endl;

uniRequest::uniRequest(int _lfd, int _cfd, int _efd): m_lfd(_lfd), m_cfd(_cfd), m_efd(_efd){}
uniRequest::uniRequest(){
    m_lfd = -1; m_cfd = -1; m_efd = -1;
}
uniRequest::~uniRequest(){
    cout << "go to destory" << endl;
}

int uniRequest::get_efd(){return m_efd;}
int uniRequest::get_cfd(){return m_cfd;}
int uniRequest::get_lfd(){return m_lfd;}

int uniRequest::acceptLink(){
    int res;

    cout << "accept link, pid is:" << std::this_thread::get_id() << endl;
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    int cfd = accept(m_lfd, (struct sockaddr *)&cliaddr, &clilen);
    if(cfd < 0 ) cout << "accept link error" << endl;
    
    res = setSocketNonBlocking(cfd);
    if(res < 0) cout << cfd << "set non block error" << endl;
    
    // printf("received from %s at PORT %d\n", 
    //         inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)), 
    //         ntohs(cliaddr.sin_port));

    // 新cfd挂上树
    struct epoll_event temp;
    temp.events = EPOLLIN | EPOLLET; 
    temp.data.ptr = static_cast<void *>(new uniRequest(m_lfd, cfd, m_efd));
    res = epoll_ctl(m_efd, EPOLL_CTL_ADD, cfd, &temp);

    return 0;
}

int uniRequest::epollIn(){
    cout << "handle cfd epoll in, pid is: " << std::this_thread::get_id() << endl;

    // 检测用返回值
    int res;

    // 读取一行http协议， 拆分， 获取 get 文件名 协议号	
	char line[1024] = {0};
	char method[16], path[256], protocol[16]; 
	
	int len = get_line(m_cfd, line, sizeof(line)); //读 http请求协议首行 GET /hello.c HTTP/1.1

    if(len == 0){  // cfd连接关闭
        res = epoll_ctl(m_efd, EPOLL_CTL_DEL, m_cfd, NULL);
        close(m_cfd);
        printf("client[%d] closed connection\n", m_cfd);
        delete this;
        return 0;
    } else{
		sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);	
		printf("method=%s, path=%s, protocol=%s\n", method, path, protocol);
		while (1) {
			char buf[1024] = {0};
			len = get_line(m_cfd, buf, sizeof(buf));	
			if (buf[0] == '\n') {
				break;	
			} else if (len == -1)
				break;
		}	
	}

    if (strncasecmp(method, "GET", 3) == 0){
		
		http_request(path);

        //http协议，交流完一次后，断开连接
        res = epoll_ctl(m_efd, EPOLL_CTL_DEL, m_cfd, NULL);
        printf("client[%d] closed connection\n", m_cfd);
        close(m_cfd);
	}
    delete this;
    return 0;
}

void uniRequest::http_request(const char *file)
{
	struct stat sbuf;
	
	// 判断文件是否存在
	int ret = stat(file, &sbuf);
	if (ret != 0) {
		// 回发浏览器 404 错误页面
        send_respond(404, "notFound", "Content-Type:html", -1);
		send_file("./404notFund.html");
	}
	
	if(S_ISREG(sbuf.st_mode)) {		// 是一个普通文件
		
		// 回发 http协议应答
		//send_respond(cfd, 200, "OK", " Content-Type: text/plain; charset=iso-8859-1", sbuf.st_size);	 
		send_respond(200, "OK", "Content-Type:image/jpeg", -1);
		//send_respond(cfd, 200, "OK", "audio/mpeg", -1);
		
		// 回发 给客户端请求数据内容。
		send_file(file);
	}	
}

void uniRequest::send_respond(int no, const char *disp, const char *type, int len)
{
	char buf[4096] = {0};
	
	sprintf(buf, "HTTP/1.0 %d %s\r\n", no, disp);
	send(m_cfd, buf, strlen(buf), 0);
	
	sprintf(buf, "Content-Type: %s\r\n", type);
	sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len);
	send(m_cfd, buf, strlen(buf), 0);
	
	send(m_cfd, "\r\n", 2, 0);
}

void uniRequest::send_file(const char *file)
{
	int n = 0, ret;
	char buf[4096] = {0};
	
	// 打开的服务器本地文件。  --- cfd 能访问客户端的 socket
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		file = "./404notFund.html";
        fd = open(file, O_RDONLY);
	}
	
	while ((n = read(fd, buf, sizeof(buf))) > 0) {		
		ret = send(m_cfd, buf, n, 0);
		if (ret == -1) {
			cout << "client closed: " << m_cfd << endl;
            return;
		}
		if (ret < 4096)		
			printf("-----send ret: %d\n", ret);
	}
}