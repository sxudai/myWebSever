#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include<string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <thread>
#include <iostream>
#include <unordered_map>

#include "util.h"
#include "request.h"

using std::cout;
using std::endl;

uniRequest::uniRequest(int _lfd, int _cfd, int _efd): m_lfd(_lfd), m_cfd(_cfd), m_efd(_efd), m_error(dsx::OK){}
uniRequest::uniRequest(): m_lfd(0), m_cfd(0), m_efd(0), m_error(dsx::invalidDefaultConstructor){}
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

	analysis_request();
	if(m_error != dsx::OK) return disconnect();
	cout << "analysis_request end" << endl;
	for(auto & p: m_heads) cout << p.first << ": " << p.second << endl;

	http_request();
	
    return disconnect();
}

void uniRequest::analysis_request(){
    // 读取一行http协议， 拆分， 获取 get 文件名 协议号	
	char line[1024], method[16], path[256], protocol[16]; 
	memset(line, 0, sizeof(line));
	memset(method, 0, sizeof(method));
	memset(path, 0, sizeof(path));
	memset(protocol, 0, sizeof(protocol));
	
	int len = get_line(m_cfd, line, sizeof(line)); //读http请求协议首行 GET /hello.c HTTP/1.0
	// cfd连接关闭
    if(len == 0){  
        m_error = dsx::anaRequestErrCfdClose;
        return;
    }

	sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);
	printf("method=%s, path=%s, protocol=%s\n", method, path, protocol);
	m_path = '.';
	m_method = method; m_path += path; m_protocol = protocol;
	while(true){
		memset(line, 0, sizeof(line));
		len = get_line(m_cfd, line, sizeof(line));	
		if (len == -1 || line[0] == '\0') break;
		char key[32], value[512];
		sscanf(line, "%[^: ]%*[: ]%[^ ]", key, value);
		m_heads.emplace(key, value);
	}
	while(true){
		memset(line, 0, sizeof(line));
		len = get_line(m_cfd, line, sizeof(line));
		// len == -1说明body没了
		if(len == -1) break;
		m_body += line;
		m_body += "\n";
	}
	return;
}

void uniRequest::http_request()
{
	if(m_method == "GET"){
		http_get();
	} else if(m_method == "POST"){
		http_post();
	}
}

void uniRequest::http_get(){
	cout << "m_method: get" << m_body << endl;
	struct stat sbuf;
	// 判断文件是否存在
	int ret = stat(m_path.c_str(), &sbuf);
	if (ret != 0) {
		// 回发浏览器 404 错误页面
        send_respond(404, "notFound", "Content-Type:html", -1);
		send_file("./404notFund.html");
		m_error = dsx::httpGetErr404;
		return;
	}

	if(S_ISREG(sbuf.st_mode)) {		// 是一个普通文件
		
		// 回发 http协议应答 
		send_respond(200, "OK", "Content-Type: image/jpeg", -1);
		
		// 回发 给客户端请求数据内容。
		send_file(m_path.c_str());
	}
}

void uniRequest::http_post(){
	cout << "m_method: post, receive content:" << m_body << endl;
	send_respond(200, "OK", "Content-Type: text/plain", -1);
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
        fd = open("./404notFund.html", O_RDONLY);
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

int uniRequest::disconnect(){
	int returnVal=0;
	int res = epoll_ctl(m_efd, EPOLL_CTL_DEL, m_cfd, NULL);
	printf("client[%d] closed connection\n", m_cfd);
	close(m_cfd);
	if(m_error != dsx::OK) return returnVal=-1;
	delete this;
	return returnVal;
}
