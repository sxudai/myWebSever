#pragma once

int get_line(int cfd, char *buf, int size);
void handle_for_sigpipe();
int setSocketNonBlocking(int fd);