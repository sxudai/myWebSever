#pragma once

#define BUFFER_SIZE 4096

// 用于非阻塞返回-1时多尝试几次
#define TRY_READ 10000;

#define SERV_PORT 80
#define OPEN_MAX 65535
#define THREADNUM 5
#define BUFLEN 4096

namespace dsx{
    enum myErrorNum{
        OK=0,
        invalidDefaultConstructor=1,
        anaRequestErrCfdClose=2,
        httpGetErr404=3
    };
}

enum CHECK_STATE { 
    CHECK_STATE_INIT = 0,
    CHECK_STATE_REQUESTLINE, //解析url
    CHECK_STATE_HEADER, //解析头部
    CHECK_STATE_CONTENT, //post报文，接收实体
    CHECK_STATE_HANDLE // 业务处理
};

enum LINE_STATUS { 
    LINE_OK = 0, 
    LINE_BAD, 
    LINE_OPEN 
};

enum HTTP_CODE { 
    NO_REQUEST = 0,  // 请求不完整，请继续读socket
    GET_REQUEST,  // 获得一个完整是请求
    BAD_REQUEST,  // 请求有错
    FORBIDDEN_REQUEST,  // 客户不具有请求访问权限
    INTERNAL_ERROR,  // 内部错误
    CLOSED_CONNECTION  // 客户端已关闭
};