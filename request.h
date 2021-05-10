#pragma once

struct uniRequest{
public:
    uniRequest();
    uniRequest(int _lfd, int _cfd, int _efd);
    uniRequest(uniRequest &) = delete;
    uniRequest& operator=(uniRequest &) = delete;
    ~uniRequest();

    int acceptLink();

    int epollIn();
    void http_request(const char *file);
    void send_file(const char *file);
    void send_respond(int no, const char *disp, const char *type, int len);

    int get_efd();
    int get_cfd();
    int get_lfd();
private:
    int m_lfd;
    int m_cfd;
    int m_efd;
};