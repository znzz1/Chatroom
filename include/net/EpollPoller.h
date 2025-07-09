#pragma once
#include <sys/epoll.h>
#include <vector>

class EpollPoller {
public:
    explicit EpollPoller(int maxEvents = 1024);
    ~EpollPoller();
    void addFd(int fd, uint32_t events);
    void modifyFd(int fd, uint32_t events);
    void removeFd(int fd);
    std::vector<epoll_event> poll(int timeoutMs = -1);
    
    // 别名方法
    void modFd(int fd, uint32_t events) { modifyFd(fd, events); }
    void delFd(int fd) { removeFd(fd); }

private:
    int epfd_;
    std::vector<epoll_event> events_;
};
