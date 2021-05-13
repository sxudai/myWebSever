#include "util.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

// 获取一行以\n或者\r\n 结尾的数据，返回数据不带\n
int get_line(int cfd, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size-1) && (c != '\n')) {
        n = recv(cfd, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(cfd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {              
                    recv(cfd, &c, 1, 0);
                } else {
                    c = '\n';
                }
            }
            if(c == '\n') break;
            buf[i] = c;
            i++;
        } else {
            c = '\n';
        }
    }
    buf[i] = '\0';
    
    if (-1 == n)
    	i = n;

    return i;
}

int setSocketNonBlocking(int fd){
    int flag = fcntl(fd, F_GETFL);
    if(flag == -1) return -1;
    flag |= O_NONBLOCK;
    if((fcntl(fd, F_SETFL, flag)) == -1){
        return -1;
    } else {
        return 0;
    }
}

void handle_for_sigpipe()
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, 0))
        return;
}