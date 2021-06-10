#pragma once

#include <memory>
// #include <unordered_map>

#include "dsx.h"
#include "request.h"

class fd2UniRequest{
private:
    // static std::unordered_map<int, std::shared_ptr<uniRequest>> fd2req;
    static std::shared_ptr<uniRequest> fd2req[OPEN_MAX+1];
public:
    static void setUniRequest(int fd, std::shared_ptr<uniRequest> SP_req);
    static void deleteUniRequest(int fd);
    static std::shared_ptr<uniRequest> getUniRequest(int fd);
    // static int getMapSize();
};