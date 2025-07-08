#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(12345);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen"); return 1;
    }

    std::cout << "Echo server listening on port 12345...\n";

    while (true) {
        sockaddr_in client{};
        socklen_t len = sizeof(client);
        int conn_fd = accept(listen_fd, (sockaddr*)&client, &len);
        if (conn_fd < 0) { perror("accept"); continue; }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));
        std::cout << "Client connected: " << ip << ':' << ntohs(client.sin_port) << '\n';

        char buf[4096];
        ssize_t n;
        while ((n = read(conn_fd, buf, sizeof(buf))) > 0) {
            write(conn_fd, buf, n);  // echo back
        }
        close(conn_fd);
        std::cout << "Client disconnected\n";
    }
    close(listen_fd);
    return 0;
}
