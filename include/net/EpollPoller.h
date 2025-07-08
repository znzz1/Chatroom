#pragma once
#include <sys/epoll.h>
#include <vector>

class EpollPoller {
public:
    explicit EpollPoller(int maxEvents = 1024);
    ~EpollPoller();
    void addFd(int fd, uint32_t events);
    void modFd(int fd, uint32_t events);
    void delFd(int fd);
    std::vector<epoll_event> poll(int timeoutMs = -1);

private:
    int epfd_;
    std::vector<epoll_event> events_;
};
