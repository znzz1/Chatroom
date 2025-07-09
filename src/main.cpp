#include "net/EpollPoller.h"
#include "net/SocketUtil.h"
#include "net/Connection.h"
#include "threadpool/ThreadPool.h"
#include "dao/DaoFactory.h"
#include "service/ChatService.h"
#include "handler/MessageHandler.h"
#include "protocol/MessageType.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <jsoncpp/json/json.h>

constexpr uint16_t PORT = 12345;

class ChatRoomServer {
private:
    int listen_fd_;
    EpollPoller poller_;
    ThreadPool thread_pool_;
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
    std::mutex connections_mutex_;
    
    std::shared_ptr<ChatService> chat_service_;
    std::unique_ptr<MessageHandler> message_handler_;
    
    class FileDescriptor {
    public:
        explicit FileDescriptor(int fd) : fd_(fd) {}
        ~FileDescriptor() { if (fd_ >= 0) close(fd_); }
        int get() const { return fd_; }
        int release() { int tmp = fd_; fd_ = -1; return tmp; }
    private:
        int fd_;
    };
    
public:
    ChatRoomServer() 
        : thread_pool_(4) {
        setupServer();
        setupServices();
    }
    
    void run() {
        std::cout << "ChatRoom server listening on port " << PORT << std::endl;
        
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
            throw std::runtime_error("socket creation failed: " + std::string(strerror(errno)));
        }
        
        int opt = 1;
        if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close(listen_fd_);
            throw std::runtime_error("setsockopt failed: " + std::string(strerror(errno)));
        }
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(PORT);
        
        if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(listen_fd_);
            throw std::runtime_error("bind failed: " + std::string(strerror(errno)));
        }
        
        if (listen(listen_fd_, SOMAXCONN) < 0) {
            close(listen_fd_);
            throw std::runtime_error("listen failed: " + std::string(strerror(errno)));
        }
        
        setNonBlocking(listen_fd_);
        poller_.addFd(listen_fd_, EPOLLIN | EPOLLET);
    }
    
    void setupServices() {
        auto factory = DaoFactory::getInstance();
        auto user_dao = factory->createUserDao(DaoType::MEMORY);
        auto room_dao = factory->createRoomDao(DaoType::MEMORY);
        chat_service_ = std::make_shared<ChatService>(std::move(user_dao), std::move(room_dao));
        message_handler_ = std::make_unique<MessageHandler>(chat_service_);
    }
    
    void handleEvent(const epoll_event& ev) {
        int fd = ev.data.fd;
        
        if (fd == listen_fd_) {
            handleNewConnection();
        } else {
            if (ev.events & EPOLLIN) {
                handleReadEvent(fd);
            }
            if (ev.events & EPOLLOUT) {
                handleWriteEvent(fd);
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
            
            auto conn = std::make_shared<Connection>(conn_fd);
            
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_[conn_fd] = conn;
            }
            
            poller_.addFd(conn_fd, EPOLLIN | EPOLLET);
            
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
            std::cout << "Client connected: " << ip << ':' << ntohs(cli.sin_port) << " (fd: " << conn_fd << ")" << std::endl;
        }
    }
    
    void handleReadEvent(int fd) {
        thread_pool_.addTask([this, fd]() {
            this->processReadTask(fd);
        });
    }
    
    void handleWriteEvent(int fd) {
        thread_pool_.addTask([this, fd]() {
            this->processWriteTask(fd);
        });
    }
    
    void processReadTask(int fd) {
        char buf[4096];
        
        while (true) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                auto it = connections_.find(fd);
                if (it != connections_.end()) {
                    it->second->appendToReadBuffer(buf, n);
                }
            } else if (n == 0) {
                handleConnectionClose(fd);
                return;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    std::cerr << "Read error on fd " << fd << ": " << strerror(errno) << std::endl;
                    handleConnectionClose(fd);
                    return;
                }
            }
        }
        
        // 处理读缓冲区中的完整消息
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            auto conn = it->second;
            
            // 提取所有完整的消息
            auto messages = conn->extractMessages();
            for (const auto& message : messages) {
                processMessage(fd, message);
            }
        }
    }
    
    void processWriteTask(int fd) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            return;
        }
        
        trySendData(fd);
    }
    
    void processMessage(int fd, const Message& message) {
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            try {
                message_handler_->handleMessage(message, it->second);
            } catch (const std::exception& e) {
                std::cerr << "Error processing message: " << e.what() << std::endl;
                // MessageHandler 内部会处理错误响应
            }
        }
    }
    
    void trySendData(int fd) {
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            return;
        }
        
        auto conn = it->second;
        
        while (conn->getWriteBufferSize() > 0) {
            std::string data = conn->extractWriteBuffer(4096);
            if (data.empty()) {
                break;
            }
            
            ssize_t n = write(fd, data.c_str(), data.length());
            if (n > 0) {
                // 数据已发送
            } else if (n == 0) {
                handleConnectionClose(fd);
                return;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 缓冲区满，等待下次写事件
                    poller_.modifyFd(fd, EPOLLIN | EPOLLOUT | EPOLLET);
                    return;
                } else {
                    std::cerr << "Write error on fd " << fd << ": " << strerror(errno) << std::endl;
                    handleConnectionClose(fd);
                    return;
                }
            }
        }
        
        // 没有更多数据要发送，移除写事件监听
        poller_.modifyFd(fd, EPOLLIN | EPOLLET);
    }
    
    void handleConnectionError(int fd) {
        std::cerr << "Connection error on fd " << fd << std::endl;
        handleConnectionClose(fd);
    }
    
    void handleConnectionClose(int fd) {
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            auto it = connections_.find(fd);
            if (it != connections_.end()) {
                // 通知聊天服务用户断开连接
                chat_service_->handleUserDisconnect(fd);
                connections_.erase(it);
            }
        }
        
        poller_.removeFd(fd);
        close(fd);
        std::cout << "Client disconnected (fd: " << fd << ")" << std::endl;
    }
};

int main() {
    try {
        ChatRoomServer server;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
