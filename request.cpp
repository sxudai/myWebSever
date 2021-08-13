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
#include <memory>
#include <iostream>
#include <unordered_map>
#include <chrono>

#include "fd2UniRequest.h"
#include "util.h"
#include "timer.h"
#include "request.h"
#include "dsx.h"
#include "singleton.h"
#include "logger.h"

extern TimerManager timerManag;

using std::cout;
using std::endl;

uniRequest::uniRequest(int _lfd, int _cfd, int _efd): m_lfd(_lfd), m_cfd(_cfd), m_efd(_efd), m_timeout(600000), m_error(dsx::OK){memset( buffer, '\0', BUFFER_SIZE );}
uniRequest::uniRequest(int _lfd, int _cfd, int _efd, int _timeout): m_lfd(_lfd), m_cfd(_cfd), m_efd(_efd), m_timeout(_timeout), m_error(dsx::OK){memset( buffer, '\0', BUFFER_SIZE );}
uniRequest::uniRequest(): m_lfd(0), m_cfd(0), m_efd(0), m_error(dsx::invalidDefaultConstructor){}
uniRequest::~uniRequest(){
	// std::this_thread::sleep_for(std::chrono::seconds(240));
	// close(m_cfd);
	// printf("uniRequest::~uniRequest(), cfd closed, this point is %p\n", this);
}

int uniRequest::get_efd(){return m_efd;}
int uniRequest::get_cfd(){return m_cfd;}
int uniRequest::get_lfd(){return m_lfd;}
uniRequest::SP_TimerNode uniRequest::get_timer(){return m_timer.lock();}

int uniRequest::acceptLink(){
    int res;

    // cout << "accept link, pid is:" << std::this_thread::get_id() << endl;
	int cfd;
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);

	while (true)
	{
		cfd = accept(m_lfd, (struct sockaddr *)&cliaddr, &clilen);
		if(cfd < 0) {
			if(errno == EAGAIN){
				break;
			} else {
				printf("errno is: %d\n", errno);
				break;
			}
		} else if(cfd > OPEN_MAX+1){
			close(cfd);
			printf("too many client, undingable!!!!, give up connection\n");
			return -1;
		}
		
		res = setSocketNonBlocking(cfd);
		if(res < 0) {
			printf("uniRequest::acceptLink(), %d: set non block error\n", cfd);
			break;
		}

		// 新request
		std::shared_ptr<uniRequest> newreq = std::make_shared<uniRequest>(m_lfd, cfd, m_efd);
		timerManag.addTimer(newreq, m_timeout);
		printf("func uniRequest::acceptLink(), timer ptr: %p\n", (newreq->get_timer()).get());
		printf("func uniRequest::acceptLink(), newreq ptr: %p\n", newreq.get());
		// 加入fd2req
		fd2UniRequest::setUniRequest(cfd, newreq);

		// 新cfd挂上树
		struct epoll_event temp;
		temp.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
		temp.data.fd = cfd;
		res = epoll_ctl(m_efd, EPOLL_CTL_ADD, cfd, &temp);
	}
	printf("accept link end\n");
	
    return 0;
}

int uniRequest::linkTimer(SP_TimerNode _timer){
	m_timer = _timer;
	return 0;
}

void uniRequest::resetReq(){
	m_method.clear();
    m_path.clear();
    m_protocol.clear();
    m_body.clear();
    m_heads.clear();
	m_error = dsx::OK;

	memset( buffer, '\0', BUFFER_SIZE );
    read_index = 0;
    checked_index = 0;
    start_line = 0;
    checkstate = CHECK_STATE_INIT;
}

void uniRequest::seperateTimer(){
	SP_TimerNode req_timer = m_timer.lock();
	if(req_timer) {
		printf("func uniRequest::seperateTimer(), timer ptr: %p\n", req_timer.get());
		printf("func uniRequest::seperateTimer(), this ptr: %p\n", this);
		req_timer->clearReq();
	}
	m_timer.reset();
}

int uniRequest::epollIn(){
	checkstate = CHECK_STATE_REQUESTLINE;
	int data_read = 0;
	printf("epollIn\n");
	while(true) {
		int no_req_cnt = TRY_READ;
		int try_to_read = TRY_READ;
		while(try_to_read){
			data_read = recv( m_cfd, buffer + read_index, BUFFER_SIZE - read_index, 0 );
			printf("%d readed\n", data_read);
			if ( data_read == -1 ) {
				// socket空，多尝试几次
				--try_to_read;
			}
			else if ( data_read == 0 ) {
				//对端关闭，本端直接进入关闭流程
				return disconnect();
			} else {
				// 读到了，跳出recv循环，开始处理消息
				break;
			}
		}
		if( data_read == -1 ) {
			// 读了几次了，对面还是不写东西过来，直接放弃连接
			return disconnect();
		}

        read_index += data_read;
        HTTP_CODE result = parse_content();
        // 读的不完整，接着读
		if( result == NO_REQUEST ) {
			--no_req_cnt;
			// 读老半天了还没读到东西，直接毁灭吧
			if(no_req_cnt == 0) return disconnect();
			continue;
		} else if(result == GET_REQUEST) {
			// 报文分析处理完毕
			// 跳出循环处理请求
			break;
		} else{
			// 分析出错，直接放弃连接
			return disconnect();
		}
    }
	// printf("heads are:\n");
	// for(auto & p: m_heads) cout << p.first << ": " << p.second << endl;

	http_request();
	handle_connect();
	printf("epoll in end\n");
	
    return 0;
}

HTTP_CODE uniRequest::parse_content(){
    LINE_STATUS linestatus = LINE_OK;
    HTTP_CODE retcode = NO_REQUEST;
    while( ( linestatus = parse_line() ) == LINE_OK )
    {
		// szTemp指向新一行的第一个字符
        char* szTemp = buffer + start_line;
        start_line = checked_index;
        switch (checkstate)
        {
            case CHECK_STATE_REQUESTLINE: {
                retcode = parse_requestline(szTemp);
                if ( retcode == BAD_REQUEST )
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                retcode = parse_headers( szTemp );
                if ( retcode == BAD_REQUEST ) {
                    return BAD_REQUEST;
                } else if ( retcode == GET_REQUEST ) {
                    return GET_REQUEST;
                }
                break;
            }
			case CHECK_STATE_CONTENT:{
				retcode = recv_body( szTemp );
				if ( retcode == BAD_REQUEST ) {
                    return BAD_REQUEST;
                } else if ( retcode == GET_REQUEST ) {
                    return GET_REQUEST;
                }
				break;
			}
			case CHECK_STATE_HANDLE:{
				return GET_REQUEST;
			}
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    if( linestatus == LINE_OPEN ){
        return NO_REQUEST;
    }
    else {
        return BAD_REQUEST;
    }
}

HTTP_CODE uniRequest::recv_body(char* szTemp) {
	if(!m_heads.count("Content-Length")) return BAD_REQUEST;
	if(m_heads.count("Content-Length") > strlen(szTemp)) return NO_REQUEST;
	m_body = szTemp;
	checkstate = CHECK_STATE_HANDLE;
	return GET_REQUEST;
}

// parse_line将检测\r\n替换为\0\0，后将check_index指向下一字符
LINE_STATUS uniRequest::parse_line() {
	// 状态是CHECK_STATE_CONTENT，post接受实体，不以\r\n结尾，因此无法判断，直接返回LINE_OK
	// 状态是CHECK_STATE_HANDLE，头部解析已经完毕，直接返回LINE_OK
	// 不会有以上问题，以防万一
	if(checkstate == CHECK_STATE_CONTENT || checkstate == CHECK_STATE_HANDLE){
		return LINE_OK;
	}
    char temp;
    for ( ; checked_index < read_index; ++checked_index )
    {
        temp = buffer[ checked_index ];
        if ( temp == '\r' )
        {
            if ( ( checked_index + 1 ) == read_index ) {
                return LINE_OPEN;
            }  else if ( buffer[ checked_index + 1 ] == '\n' ) {
                buffer[ checked_index++ ] = '\0';
                buffer[ checked_index++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if( temp == '\n' ) {
            if( ( checked_index > 1 ) &&  buffer[ checked_index - 1 ] == '\r' ) {
                buffer[ checked_index-1 ] = '\0';
                buffer[ checked_index++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HTTP_CODE uniRequest::parse_requestline( char* szTemp){
	printf("szTemp is: %s\n", szTemp);
	/*
	这部分的效果相当于把
	get http://127.0.1:8000/a.jpeg http/1.1\r\n.....
	变成
	get\0http://127.0.1:8000/a.jpeg http/1.1\r\n.....
	其中
	char* szURL指向h
	char* sztemp指向g
	*/
    char* szURL = strpbrk(szTemp, " \t" );
    if ( ! szURL ) {
        return BAD_REQUEST;
    }
    *szURL++ = '\0';
	

    char* szMethod = szTemp;
    if ( strcasecmp( szMethod, "GET" ) == 0 ){
		m_method = "GET";
        printf( "The request method is GET\n" );
    } else if(strcasecmp( szMethod, "POST" ) == 0 ){
		m_method = "POST";
		printf( "The request method is POST\n" );
	} else {
        return BAD_REQUEST;
    }

	// 去除get和http之间可能多的空格或制表符
    szURL += strspn( szURL, " \t" );

	/*
	这部分的效果相当于把
	get\0http://127.0.1:8000/a.jpeg http/1.1\r\n.....
	变成
	get\0http://127.0.1:8000/a.jpeg\0http/1.1\r\n.....
	其中
	char* szURL指向第一个h
	char* szVersion指向第二个h
	*/
    char* szVersion = strpbrk( szURL, " \t" );
    if ( ! szVersion ){
		printf( "! szVersion, bad request\n" );
        return BAD_REQUEST;
    }
    *szVersion++ = '\0';
    szVersion += strspn( szVersion, " \t" );
    if ( strcasecmp( szVersion, "HTTP/1.1" ) == 0 ) {
		m_protocol = "HTTP/1.1";
    } else if(strcasecmp( szVersion, "HTTP/1.0" ) == 0 ){
		m_protocol = "HTTP/1.0";
	} else{
		printf( "invalid http Version, %s\n", szVersion);
		return BAD_REQUEST;
	}

	printf( "http version %s\n", szVersion );

	//网址跳掉，保留文件路径
    if ( strncasecmp( szURL, "http://", 7 ) == 0 ) {
        szURL += 7;
        szURL = strchr( szURL, '/' );
    }

    if ( ! szURL || szURL[ 0 ] != '/' ) {
        return BAD_REQUEST;
    }
	m_path = ".";
	m_path += szURL;
    printf( "The request URL is: %s\n", m_path.c_str() );
    checkstate = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTP_CODE uniRequest::parse_headers( char* szTemp )
{
    if ( szTemp[0] == '\0' ) {
		if(m_path == "POST"){
			// post接受实体
			checkstate = CHECK_STATE_CONTENT;
		} else{
			// get开始处理事务
			checkstate = CHECK_STATE_HANDLE;
			return GET_REQUEST;
		}
	// 否则是一个合理的头部，解析选项
    } else {
		char key[32], value[512];
		sscanf(szTemp, "%[^: ]%*[: ]%[^ ]", key, value);
		m_heads.emplace(key, value);
	}

    return NO_REQUEST;
}

int uniRequest::handle_connect(){
	if(m_heads.count("Connection") && m_heads["Connection"] == "keep-alive"){
		printf("uniRequest::handle_connect() keep-alive\n");
		// 先添加timer
		timerManag.addTimer(shared_from_this(), m_timeout);
		resetReq();
		// epoll oneshot， cfd重新上树
		struct epoll_event temp;
		temp.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
		temp.data.fd = m_cfd;
		int res = epoll_ctl(m_efd, EPOLL_CTL_MOD, m_cfd, &temp);
		printf("res is: %d\n", res);
		printf("new timer is：%p\n", m_timer.lock().get());
		return 1;
	}
	printf("uniRequest::handle_connect() disconnect\n");
	disconnect();
	return 0;
}

void uniRequest::http_request()
{
	cout << "m_method: " << m_method << endl;
	LOG << "m_method: " << m_method;
	if(m_method == "GET"){
		printf("method get\n");
		http_get();
	} else if(m_method == "POST"){
		printf("method post\n");
		http_post();
	}
}

void uniRequest::http_get(){
	printf("uniRequest::http_get()\n");
	struct stat sbuf;
	// 判断文件是否存在
	int ret = stat(m_path.c_str(), &sbuf);
	if (ret != 0) {
		// 回发浏览器 404 错误页面
		printf("file not exist\n");
        send_respond(404, "notFound", "Content-Type:html", -1);
		send_file("./404notFund.html");
		m_error = dsx::httpGetErr404;
		return;
	}

	if(S_ISREG(sbuf.st_mode)) {		// 是一个普通文件
		
		// 回发 http协议应答
		std::string head = "";

		std::string Content_Type = m_path.substr(m_path.rfind('.'));
		cout << Content_Type << endl;
		LOG << Content_Type;
		Content_Type = MimeType::getMime(Content_Type);

		head += "Content-Type: " + Content_Type + "\r\n";

		head += "Content-Length: " + std::to_string(sbuf.st_size) + "\r\n";

		cout<< "head is: " << head <<endl;
		LOG<< "head is: " << head;
		send_respond(200, "OK", head.c_str(), -1);
		
		// 回发 给客户端请求数据内容。
		send_file(m_path.c_str());
	}
}

void uniRequest::http_post(){
	cout << "m_method: post, receive content:" << m_body << endl;
	LOG << "m_method: post, receive content:" << m_body;
	send_respond(200, "OK", "Content-Type: text/plain", -1);
}

void uniRequest::send_respond(int no, const char *disp, const char *head, int len)
{
	char buf[4096] = {0};
	
	sprintf(buf, "HTTP/1.1 %d %s\r\n", no, disp);
	send(m_cfd, buf, strlen(buf), 0);
	
	// sprintf(buf, "Content-Type: %s\r\n", type);
	// sprintf(buf+strlen(buf), "Content-Length:%d\r\n", len);
	send(m_cfd, head, strlen(head), 0);
	
	send(m_cfd, "\r\n", 2, 0);
}

void uniRequest::send_file(const char *file){
	int n = 0, ret;
	char buf[4096] = {0};
	
	// 打开的服务器本地文件。  --- cfd 能访问客户端的 socket
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
        fd = open("./404notFund.html", O_RDONLY);
	}
	
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		int cnt = 1;
		do{
			std::this_thread::sleep_for(std::chrono::seconds(cnt));
			ret = send(m_cfd, buf, n, 0);
			++cnt;
			if(cnt == 6) break;
			if(ret > 0) {
				cout << "send " << ret << "bytes, success in cnt = " << cnt << '\n';
			}
		}while(ret == -1); 
	}
	close(fd);
}

int uniRequest::disconnect(){
	int returnVal=0;
	/*
	先删除（reset）shared_ptr
	再把cfd从efd上摘下来
	最后再close(cfd)
	保证安全
	*/
	fd2UniRequest::deleteUniRequest(m_cfd);
	
	int res = epoll_ctl(m_efd, EPOLL_CTL_DEL, m_cfd, NULL);
	printf("client[%d] closed connection\n", m_cfd);
	if(m_error != dsx::OK) {
		cout << "error: " << m_error << endl;
		LOG << "error: " << m_error;
		returnVal=-1;
	}
	close(m_cfd);
	return returnVal;
}

CHECK_STATE uniRequest::check_checkstate(){
	return checkstate;
}

void uniRequest::set_CHECK_STATE_REQUESTLINE(){
	checkstate = CHECK_STATE_REQUESTLINE;
}