#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include "timer.h"

namespace dsx{
    enum myErrorNum{
        OK=0,
        invalidDefaultConstructor=1,
        anaRequestErrCfdClose=2,
        httpGetErr404=3
    };
}

class TimerNode;

class uniRequest{
public:
    uniRequest();
    uniRequest(int _lfd, int _cfd, int _efd);
    uniRequest(int _lfd, int _cfd, int _efd, int _timeout);
    uniRequest(uniRequest &) = delete;
    uniRequest& operator=(uniRequest &) = delete;
    ~uniRequest();

    int acceptLink();
    int linkTimer(TimerNode * _timer);

    int epollIn();
    void analysis_request();
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
    TimerNode* get_timer();

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
    TimerNode* m_timer;
};