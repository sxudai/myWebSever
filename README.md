# 简介
- 这个一个c++ web后端服务器<br>
- 使用C++11制作了一个可以自动管理线程数目的线程池<br>
- 使用epoll边沿触发+非阻塞io的reactor模式<br>
- 主线程负责io多路复用接受连接请求与处理超时连接
- 支持长短连接，与超时断连<br>
- 使用有限状态机接收http报文<br>
- 异步输出logger

# 开发环境
- OS: Ubuntu 18.04
- gcc version: 7.5.0

# 使用方法
- code中使用的端口：8000<br>
- 编译服务端：make sever<br>
- 客户端：
    >短链接可以直接使用webbench<br>
    >可以直接使用浏览请求资源<br>
