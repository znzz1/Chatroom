#include "net/EpollPoller.h"
#include "net/SocketUtil.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

constexpr uint16_t PORT = 12345;

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 65536;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    bind(listen_fd, (sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, SOMAXCONN);
    setNonBlocking(listen_fd);

    EpollPoller poller;
    poller.addFd(listen_fd, EPOLLIN | EPOLLET);

    std::cout << "Server on port "<< PORT << '\n';
    char buf[4096];

    while (true) {
        for (auto& ev : poller.poll(-1)) {
            if (ev.data.fd == listen_fd) {
                // create new connections
                while (true) {
                    sockaddr_in cli{};
                    socklen_t len = sizeof(cli);
                    int conn_fd = accept(listen_fd, (sockaddr*)&cli, &len);
                    if (conn_fd < 0) break;
                    setNonBlocking(conn_fd);
                    poller.addFd(conn_fd, EPOLLIN | EPOLLET);
                }
            } else if (ev.events & EPOLLIN) {
                // handle read events
                int fd = ev.data.fd;
                while (true) {
                    ssize_t n = read(fd, buf, sizeof(buf));
                    if (n > 0) {
                        write(fd, buf, n);
                    } else if (n == 0) {
                        poller.delFd(fd);
                        close(fd);
                        break;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    return 0;
}
