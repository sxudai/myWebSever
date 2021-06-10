#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include "timer.h"
#include "dsx.h"

class TimerNode;

// typedef std::shared_ptr<uniRequest> SP_ReqData;
// typedef std::shared_ptr<TimerNode> SP_TimerNode;
// typedef std::weak_ptr<uniRequest> WP_ReqData;
// typedef std::weak_ptr<TimerNode> WP_TimerNode;

class uniRequest: public std::enable_shared_from_this<uniRequest>{
public:
    typedef std::shared_ptr<TimerNode> SP_TimerNode;
    typedef std::weak_ptr<TimerNode> WP_TimerNode;
public:
    uniRequest();
    uniRequest(int _lfd, int _cfd, int _efd);
    uniRequest(int _lfd, int _cfd, int _efd, int _timeout);
    uniRequest(uniRequest &) = delete;
    uniRequest& operator=(uniRequest &) = delete;
    ~uniRequest();

    int acceptLink();
    int linkTimer(SP_TimerNode _timer);

    int epollIn();
    void http_request();
    void http_get();
    void http_post();
    void send_file(const char *file);
    void send_respond(int no, const char *disp, const char *type, int len);

    int handle_connect();
    void resetReq();
    void seperateTimer();

    int disconnect();

    int get_efd();
    int get_cfd();
    int get_lfd();
    SP_TimerNode get_timer();

private:
    int m_lfd;
    int m_cfd;
    int m_efd;
    int m_timeout;

    std::string m_method;
    std::string m_path;
    std::string m_protocol;
    std::string m_body;

    dsx::myErrorNum m_error;

    std::unordered_map<std::string, std::string> m_heads;
    WP_TimerNode m_timer;

public:
    HTTP_CODE parse_content();
    LINE_STATUS parse_line();
    HTTP_CODE parse_headers( char* szTemp );
    HTTP_CODE parse_requestline( char* szTemp);
    HTTP_CODE recv_body( char* szTemp);
    CHECK_STATE check_checkstate();
    void set_CHECK_STATE_REQUESTLINE();
    
private:
    char buffer[BUFFER_SIZE];
    int read_index = 0;
    int checked_index = 0;
    int start_line = 0;
    CHECK_STATE checkstate = CHECK_STATE_INIT;
};