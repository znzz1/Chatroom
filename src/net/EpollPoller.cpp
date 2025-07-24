#include "net/EpollPoller.h"
#include <stdexcept>
#include <unistd.h>
#include <errno.h>

EpollPoller::EpollPoller(int maxEvents) : events_(maxEvents) {
    epfd_ = epoll_create1(0);
    if (epfd_ < 0) throw std::runtime_error("epoll_create1 failed");
}
EpollPoller::~EpollPoller() { close(epfd_); }

bool EpollPoller::addFd(int fd, uint32_t ev) {
    epoll_event e{.events = ev, .data = {.fd = fd}};
    return epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &e) == 0;
}
bool EpollPoller::modifyFd(int fd, uint32_t ev) {
    epoll_event e{.events = ev, .data = {.fd = fd}};
    return epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &e) == 0;
}
bool EpollPoller::removeFd(int fd) {
    return epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
}
std::vector<epoll_event> EpollPoller::poll(int timeout) {
    int n = epoll_wait(epfd_, events_.data(), events_.size(), timeout);
    return std::vector<epoll_event>(events_.begin(), events_.begin() + n);
}
