#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <fcntl.h>
#include <unistd.h>

inline void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return;
    }
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

#endif