#include "net/EpollPoller.h"
#include "net/SocketUtil.h"
#include "net/Connection.h"
#include "threadpool/ThreadPool.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <unordered_map>
#include <mutex>

constexpr uint16_t PORT = 12345;

class EpollThreadPoolServer {
private:
    int listen_fd_;
    EpollPoller poller_;
    ThreadPool thread_pool_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    std::mutex connections_mutex_;
    
public:
    EpollThreadPoolServer() 
        : thread_pool_(4) {
        setupServer();
    }
    
    void run() {
        std::cout << "Server on port " << PORT << std::endl;
        
        while (true) {
            auto events = poller_.poll(-1);
            for (auto& ev : events) {
                handleEvent(ev);
            }
        }
    }
    
private:
    void setupServer() {
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            throw std::runtime_error("socket creation failed");
        }
        
        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(PORT);
        
        if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("bind failed");
        }
        
        if (listen(listen_fd_, SOMAXCONN) < 0) {
            throw std::runtime_error("listen failed");
        }
        
        setNonBlocking(listen_fd_);
        
        poller_.addFd(listen_fd_, EPOLLIN | EPOLLET);
    }
    
    void handleEvent(const epoll_event& ev) {
        int fd = ev.data.fd;
        
        if (fd == listen_fd_) {
            handleNewConnection();
        } else {
            if (ev.events & EPOLLIN) {
                handleReadEvent(fd);
            }
            if (ev.events & (EPOLLERR | EPOLLHUP)) {
                handleConnectionError(fd);
            }
        }
    }
    
    void handleNewConnection() {
        while (true) {
            sockaddr_in cli{};
            socklen_t len = sizeof(cli);
            int conn_fd = accept(listen_fd_, (sockaddr*)&cli, &len);
            
            if (conn_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    std::cerr << "Accept error: " << strerror(errno) << std::endl;
                    break;
                }
            }
            
            setNonBlocking(conn_fd);
            
            auto conn = std::make_unique<Connection>(conn_fd);
            
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_[conn_fd] = std::move(conn);
            }
            
            poller_.addFd(conn_fd, EPOLLIN | EPOLLET);
        }
    }
    
    void handleReadEvent(int fd) {
        thread_pool_.addTask([this, fd]() {
            this->processReadTask(fd);
        });
    }
    
    void processReadTask(int fd) {
        char buf[4096];
        
        while (true) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                // 将数据添加到连接的读缓冲区
                auto it = connections_.find(fd);
                if (it != connections_.end()) {
                    it->second->appendToReadBuffer(buf, n);
                }
            } else if (n == 0) {
                handleConnectionClose(fd);
                return;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // 没有更多数据
                } else {
                    std::cerr << "Read error on fd " << fd << ": " << strerror(errno) << std::endl;
                    handleConnectionClose(fd);
                    return;
                }
            }
        }
        
        // 处理读缓冲区中的数据
        auto it = connections_.find(fd);
        if (it != connections_.end() && it->second->getReadBufferSize() > 0) {
            std::string data = it->second->extractReadBuffer();
            if (!data.empty()) {
                processData(fd, data);
            }
        }
    }
    
    void processData(int fd, const std::string& data) {
        sendResponse(fd, data);
    }
    
    void sendResponse(int fd, const std::string& response) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            return;
        }
        
        // 将响应添加到写缓冲区
        it->second->appendToWriteBuffer(response);
        
        // 尝试发送数据
        trySendData(fd);
    }
    
    void trySendData(int fd) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            return;
        }
        
        char buf[4096];
        while (it->second->getWriteBufferSize() > 0) {
            std::string data = it->second->extractWriteBuffer(sizeof(buf));
            if (data.empty()) break;
            
            ssize_t n = write(fd, data.c_str(), data.length());
            if (n > 0) {
                // 如果只发送了部分数据，将剩余数据放回缓冲区
                if (static_cast<size_t>(n) < data.length()) {
                    std::string remaining = data.substr(n);
                    it->second->appendToWriteBuffer(remaining);
                    break;  // 发送缓冲区满，等待下次写事件
                }
            } else if (n == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 发送缓冲区满，将数据放回缓冲区
                    it->second->appendToWriteBuffer(data);
                    break;
                } else {
                    std::cerr << "Write error on fd " << fd << ": " << strerror(errno) << std::endl;
                    handleConnectionClose(fd);
                    return;
                }
            }
        }
        
        std::cout << "Sent response to fd " << fd << std::endl;
    }
    
    void handleConnectionError(int fd) {
        std::cout << "Connection error on fd " << fd << std::endl;
        handleConnectionClose(fd);
    }
    
        void handleConnectionClose(int fd) {
        poller_.delFd(fd);
        
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            auto it = connections_.find(fd);
            if (it != connections_.end()) {
                it->second->close();  // 使用 Connection 的 close 方法
                connections_.erase(it);
            }
        }
    }
};

int main() {
    try {
        EpollThreadPoolServer server;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
