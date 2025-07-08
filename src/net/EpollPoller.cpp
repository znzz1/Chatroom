#include "net/EpollPoller.h"
#include <stdexcept>
#include <unistd.h>

EpollPoller::EpollPoller(int maxEvents) : events_(maxEvents) {
    epfd_ = epoll_create1(0);
    if (epfd_ < 0) throw std::runtime_error("epoll_create1 failed");
}
EpollPoller::~EpollPoller() { close(epfd_); }

void EpollPoller::addFd(int fd, uint32_t ev) {
    epoll_event e{.events = ev, .data = {.fd = fd}};
    epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &e);
}
void EpollPoller::modFd(int fd, uint32_t ev) {
    epoll_event e{.events = ev, .data = {.fd = fd}};
    epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &e);
}
void EpollPoller::delFd(int fd) {
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
}
std::vector<epoll_event> EpollPoller::poll(int timeout) {
    int n = epoll_wait(epfd_, events_.data(), events_.size(), timeout);
    return std::vector<epoll_event>(events_.begin(), events_.begin() + n);
}
