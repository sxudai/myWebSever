#pragma once

int get_line(int cfd, char *buf, int size);
void handle_for_sigpipe();
void handle_for_sigpipe(int signo);
int setSocketNonBlocking(int fd);