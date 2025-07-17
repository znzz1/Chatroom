#include "server/ChatRoomServer.h"
#include "net/SocketUtil.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include "utils/EnvLoader.h"

ChatRoomServer::ChatRoomServer() 
    : listen_fd_(-1)
{
    EnvLoader::loadFromFile("config/server.env");
    port_ = EnvLoader::getInt("SERVER_PORT");
    thread_pool_ = ThreadPool(EnvLoader::getInt("THREAD_POOL_SIZE"));

    setupServer();
    setupServices();
    loadRoomsFromDatabase();
}

ChatRoomServer::~ChatRoomServer() {
    stop();
    if (listen_fd_ >= 0) {
        close(listen_fd_);
    }
}

void ChatRoomServer::run() {
    running_ = true;
    std::cout << "ChatRoom server listening on port " << port_ << std::endl;
    
    while (running_) {
        auto events = poller_.poll(EPOLL_TIMEOUT_MS); 
        for (auto& ev : events) {
            handleEvent(ev);
        }
    }
}

void ChatRoomServer::stop() {
    running_ = false;
    std::cout << "Server stopping..." << std::endl;
}

void ChatRoomServer::setupServer() {
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
    addr.sin_port = htons(port_);
    
    if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listen_fd_);
        throw std::runtime_error("bind failed: " + std::string(strerror(errno)));
    }
    
    if (listen(listen_fd_, SOMAXCONN) < 0) {
        close(listen_fd_);
        throw std::runtime_error("listen failed: " + std::string(strerror(errno)));
    }
    
    if (!setNonBlocking(listen_fd_)) {
        std::cerr << "Failed to set non-blocking mode for listen fd" << std::endl;
        return;
    }
    poller_.addFd(listen_fd_, EPOLLIN | EPOLLET);
}

void ChatRoomServer::setupServices() {
    service_manager_ = std::make_shared<ServiceManager>();
}

void ChatRoomServer::loadRoomsFromDatabase() {
    auto result = service_manager_->getAllRooms();
    if (result.ok) {
        for (const auto& room : result.data) {
            RoomInfo roomInfo(room.name, room.description, room.max_users, room.creator_id, room.created_time);

            if (room.status == RoomStatus::ACTIVE) {
                active_rooms_[room.id] = roomInfo;
            } else {
                inactive_rooms_[room.id] = roomInfo;
            }
        }
        std::cout << "Loaded " << (inactive_rooms_.size() + active_rooms_.size())
                  << " rooms from database, " << active_rooms_.size() << " of them are active." << std::endl;
    } else {
        std::cerr << "Failed to load rooms from database: " << result.error_message << std::endl;
    }
}

void ChatRoomServer::handleEvent(const epoll_event& ev) {
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

void ChatRoomServer::handleNewConnection() {
    
}

void ChatRoomServer::handleReadEvent(int fd) {
    Connection* connection = nullptr;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            handleConnectionClose(fd);
            return;
        }
        connection = it->second.get();
    }
    
    auto result = connection->recvToReadBuffer(fd, 4096);
    
    if (result.error) {
        handleConnectionError(fd);
        return;
    } else if (result.connection_closed) {
        handleConnectionClose(fd);
        return;
    }

    if (result.bytes_read == 0) return;
    auto messages = connection->extractMessages();
    for (const auto& msg : messages) {
        handleRequest(fd, msg);
    }
}

void ChatRoomServer::handleWriteEvent(int fd) {
    Connection* connection = nullptr;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            handleConnectionClose(fd);
            return;
        }
        connection = it->second.get();
    }

    auto result = connection->sendFromWriteBuffer(fd, 4096);
    
    if (result.error) {
        handleConnectionError(fd);
        return;
    }
    
    if (!result.has_more_data) poller_.modifyFd(fd, EPOLLIN);
}

void ChatRoomServer::handleConnectionError(int fd) {
    handleConnectionClose(fd);
}

void ChatRoomServer::handleConnectionClose(int fd) {
    poller_.removeFd(fd);
    ::close(fd);

    thread_pool_.enqueue([this, fd]() {
        {
            std::lock_guard lock(connections_mutex_);
            auto it = connections_.find(fd);
            if (it != connections_.end()) {
                connections_.erase(it);
            }
        }

        int userId = -1;
        {
            std::lock_guard lock(fd_to_userId_mutex_);
            auto it = fd_to_userId_.find(fd);
            if (it != fd_to_userId_.end()) {
                userId = it->second;
                fd_to_userId_.erase(it);
            }
        }
        if (userId != -1) {
            std::lock_guard lock(userId_to_fd_mutex_);
            userId_to_fd_.erase(userId);
        }

        int roomId = -1;
        if (userId != -1) {
            std::lock_guard lock(userId_to_roomId_mutex_);
            auto it = userId_to_roomId_.find(userId);
            if (it != userId_to_roomId_.end()) {
                roomId = it->second;
                userId_to_roomId_.erase(it);
            }
        }

        if (roomId != -1) {
            std::lock_guard lock(active_rooms_mutex_);
            auto it = active_rooms_.find(roomId);
            if (it != active_rooms_.end()) {
                auto &room = it->second;
                std::lock_guard lock(room.users_mutex);
                room.users.erase(userId);
            }
        }

        if (roomId != -1) notifyUserLeaveRoom(roomId, userId);
    });
}

void ChatRoomServer::handleRequest(int fd, const Message& message) {
    switch (message.type) {
        case MSG_REGISTER:
            break;
        case MSG_CHANGE_PASSWORD:
            break;
        case MSG_CHANGE_DISPLAY_NAME:
            break;
        case MSG_LOGIN:
            break;
        case MSG_LOGOUT:
            break;
        case MSG_GET_ACTIVE_ROOMS:
            break;
        case MSG_GET_ALL_ROOMS:
            break;
        case MSG_GET_ROOM_INFO:
            break;
        case MSG_CREATE_ROOM:
            break;
        case MSG_DELETE_ROOM:
            break;
        case MSG_SET_ROOM_NAME:
            break;
        case MSG_SET_ROOM_DESCRIPTION:
            break;
        case MSG_SET_ROOM_MAX_USERS:
            break;
        case MSG_SET_ROOM_STATUS:
            break;
        case MSG_SEND_MESSAGE:
            break;
        case MSG_GET_MESSAGE_HISTORY:
            break;
        case MSG_JOIN_ROOM:
            break;
        case MSG_LEAVE_ROOM:
            break;
        case MSG_GET_ROOM_MEMBERS:
            break;
        case MSG_GET_USER_INFO:
            break;
        default:
            break;
    }
}