#include <unordered_map>

#include "fd2UniRequest.h"

// std::unordered_map<int, std::shared_ptr<uniRequest>> fd2UniRequest::fd2req;
std::shared_ptr<uniRequest> fd2UniRequest::fd2req[OPEN_MAX+1];

void fd2UniRequest::setUniRequest(int fd, std::shared_ptr<uniRequest> SP_req){
    fd2req[fd] = SP_req;
}

void fd2UniRequest::deleteUniRequest(int fd){
    fd2req[fd].reset();
}

std::shared_ptr<uniRequest> fd2UniRequest::getUniRequest(int fd){
    // if(fd2req.count(fd)) return fd2req[fd];
    // // 返回空的share_ptr
    // return std::shared_ptr<uniRequest>();
    return fd2req[fd];
}

// int fd2UniRequest::getMapSize(){
//     return fd2req.size();
// }