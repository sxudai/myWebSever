#pragma once

#include <string>
#include <unordered_map>

namespace dsx{
    enum myErrorNum{
        OK=0,
        invalidDefaultConstructor=1,
        anaRequestErrCfdClose=2,
        httpGetErr404=3
    };
}


struct uniRequest{
public:
    uniRequest();
    uniRequest(int _lfd, int _cfd, int _efd);
    uniRequest(uniRequest &) = delete;
    uniRequest& operator=(uniRequest &) = delete;
    ~uniRequest();

    int acceptLink();

    int epollIn();
    void analysis_request();
    void http_request();
    void http_get();
    void http_post();
    void send_file(const char *file);
    void send_respond(int no, const char *disp, const char *type, int len);

    int disconnect();

    int get_efd();
    int get_cfd();
    int get_lfd();
private:
    int m_lfd;
    int m_cfd;
    int m_efd;

    std::string m_method;
    std::string m_path;
    std::string m_protocol;
    std::string m_body;

    dsx::myErrorNum m_error;

    std::unordered_map<std::string, std::string> m_heads;
};