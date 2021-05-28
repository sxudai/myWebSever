# myWebSever
这个一个c++ web后端服务器<br>
使用C++11制作了一个可以自动管理线程数目的线程池<br>
使用epoll边沿触发+非阻塞io<br>
支持长短连接，与超时断连<br>

# 使用方法
服务端：make sever<br>
客户端：短链接可以直接使用webbench，用于测试长连接的服务端由于比较简陋暂不提供<br>
