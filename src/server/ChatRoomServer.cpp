#include "server/ChatRoomServer.h"
#include "net/SocketUtil.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <fcntl.h>
#include <netinet/tcp.h>
#include "utils/EnvLoader.h"
#include "utils/TimeUtils.h"
#include <random>
#include <chrono>

ChatRoomServer::ChatRoomServer()
    : listen_fd_(-1)
{
    port_ = static_cast<uint16_t>(EnvLoader::getInt("SERVER_PORT").value_or(8080));
    size_t threadCount = EnvLoader::getInt("THREAD_POOL_SIZE").value_or(std::thread::hardware_concurrency());
    epoll_timeout_ms_ = EnvLoader::getInt("EPOLL_TIMEOUT_MS").value_or(1000);
    max_read_buffer_size_ = EnvLoader::getInt("MAX_READ_BUFFER_SIZE").value_or(1024 * 1024);
    max_write_buffer_size_ = EnvLoader::getInt("MAX_WRITE_BUFFER_SIZE").value_or(1024 * 1024);
    token_expire_minutes_ = EnvLoader::getInt("TOKEN_EXPIRE_MINUTES").value_or(30);
    cleanup_interval_minutes_ = EnvLoader::getInt("CLEANUP_INTERVAL_MINUTES").value_or(10);
    thread_pool_ = std::make_unique<ThreadPool>(threadCount);

    setupServer();
    setupServices();
    loadRoomsFromDatabase();

    cleanup_thread_ = std::thread([this]() {
        while (cleanup_running_) {
            std::this_thread::sleep_for(std::chrono::minutes(cleanup_interval_minutes_));
            if (cleanup_running_) cleanupExpiredTokens();
        }
    });
}

ChatRoomServer::~ChatRoomServer() {
    stop();

    cleanup_running_ = false;
    if (cleanup_thread_.joinable()) cleanup_thread_.join();

    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(active_rooms_mutex_);
        active_rooms_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(inactive_rooms_mutex_);
        inactive_rooms_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
        fd_to_userId_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(userId_to_fd_mutex_);
        userId_to_fd_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(userId_to_roomId_mutex_);
        userId_to_roomId_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(userId_to_token_mutex_);
        userId_to_token_.clear();
    }

    if (listen_fd_ >= 0) {
        close(listen_fd_);
    }
}

void ChatRoomServer::run() {
    running_ = true;
    std::cout << "ChatRoom server listening on port " << port_ << std::endl;
    
    while (running_) {
        auto events = poller_.poll(epoll_timeout_ms_); 
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

            if (room.is_active) {
                active_rooms_[room.id] = roomInfo;
            } else {
                inactive_rooms_[room.id] = roomInfo;
            }
        }
        std::cout << "Loaded " << (inactive_rooms_.size() + active_rooms_.size())
                  << " rooms from database, " << active_rooms_.size() << " of them are active." << std::endl;
    } else {
        std::cerr << "Failed to load rooms from database: " << result.message << std::endl;
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
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                std::cerr << "Failed to accept new connection: " << strerror(errno) << std::endl;
                break;
            }
        }
        
        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags < 0) {
            std::cerr << "Failed to get socket flags: " << strerror(errno) << std::endl;
            ::close(client_fd);
            continue;
        }
        
        if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            std::cerr << "Failed to set non-blocking mode: " << strerror(errno) << std::endl;
            ::close(client_fd);
            continue;
        }
        
        int opt = 1;
        if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set TCP_NODELAY: " << strerror(errno) << std::endl;
        }
        
        auto connection = std::make_shared<Connection>(client_fd);
        
        connection->setWriteEventCallback([this](int fd) {
            poller_.modifyFd(fd, EPOLLIN | EPOLLOUT);
        });
        
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_[client_fd] = connection;
        }
        
        if (!poller_.addFd(client_fd, EPOLLIN)) {
            std::cerr << "Failed to add client fd to epoll: " << strerror(errno) << std::endl;
            ::close(client_fd);
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_.erase(client_fd);
            }
            continue;
        }
    }
}

void ChatRoomServer::handleReadEvent(int fd) {
    Connection* connection = nullptr;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            thread_pool_->addTask([this, fd]() {
                cleanupConnection(fd);
            });
            return;
        }
        connection = it->second.get();
    }
    
    auto result = connection->recvToReadBuffer(fd, 4096);
    
    if (result.error) {
        handleConnectionError(fd);
        return;
    } else if (result.connection_closed) {
        cleanupConnection(fd);
        return;
    }

    if (result.bytes_read == 0) return;
    auto messages = connection->extractMessages();
    for (const auto& msg : messages) {
        thread_pool_->addTask([this, fd, msg]() {
            handleRequest(fd, msg);
        });
    }
}

void ChatRoomServer::handleWriteEvent(int fd) {
    Connection* connection = nullptr;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            thread_pool_->addTask([this, fd]() {
                cleanupConnection(fd);
            });
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
    thread_pool_->addTask([this, fd]() {
        cleanupConnection(fd);
    });
}

void ChatRoomServer::cleanupConnection(int fd) {
    int userId = -1;
    int roomId = -1;
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            connections_.erase(it);
        }
    }

    {
        std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
        auto it = fd_to_userId_.find(fd);
        if (it != fd_to_userId_.end()) {
            userId = it->second;
            fd_to_userId_.erase(it);
        }
    }
    
    if (userId != -1) {
        std::lock_guard<std::mutex> lock(userId_to_fd_mutex_);
        auto it = userId_to_fd_.find(userId);
        if (it != userId_to_fd_.end() && it->second == fd) {
            userId_to_fd_.erase(it);
        }
    }

    if (userId != -1) {
        std::lock_guard<std::mutex> lock(userId_to_roomId_mutex_);
        auto it = userId_to_roomId_.find(userId);
        if (it != userId_to_roomId_.end()) {
            roomId = it->second;
            userId_to_roomId_.erase(it);
        }
    }

    if (roomId != -1) {
        std::lock_guard<std::mutex> lock(active_rooms_mutex_);
        auto it = active_rooms_.find(roomId);
        if (it != active_rooms_.end()) {
            it->second.users.erase(userId);
        }
    }


    poller_.removeFd(fd);
    ::close(fd);


    if (roomId != -1) {
        Json::Value notification;
        notification["user_id"] = userId;
        notification["room_id"] = roomId;
        notifyRoomUsers(roomId, MSG_USER_LEAVE_PUSH, notification);
    }    
}

void ChatRoomServer::handleRequest(int fd, const NetworkMessage& message) {
    switch (message.type) {
        case MSG_REGISTER: 
            handleRegister(fd, message.data);
            break;
        case MSG_CHANGE_PASSWORD:
            handleChangePassword(fd, message.data);
            break;
        case MSG_CHANGE_DISPLAY_NAME:
            handleChangeDisplayName(fd, message.data);
            break;
        case MSG_LOGIN:
            handleLogin(fd, message.data);
            break;
        case MSG_LOGOUT:
            handleLogout(fd, message.data);
            break;
        case MSG_FETCH_ACTIVE_ROOMS:
            handleFetchActiveRooms(fd, message.data);
            break;
        case MSG_FETCH_INACTIVE_ROOMS:
            handleFetchInactiveRooms(fd, message.data);
            break;
        case MSG_CREATE_ROOM:
            handleCreateRoom(fd, message.data);
            break;
        case MSG_DELETE_ROOM:
            handleDeleteRoom(fd, message.data);
            break;
        case MSG_SET_ROOM_NAME:
            handleSetRoomName(fd, message.data);
            break;
        case MSG_SET_ROOM_DESCRIPTION:
            handleSetRoomDescription(fd, message.data);
            break;
        case MSG_SET_ROOM_MAX_USERS:
            handleSetRoomMaxUsers(fd, message.data);
            break;
        case MSG_SET_ROOM_STATUS:
            handleSetRoomStatus(fd, message.data);
            break;
        case MSG_SEND_MESSAGE:
            handleSendMessage(fd, message.data);
            break;
        case MSG_GET_MESSAGE_HISTORY:
            handleGetMessageHistory(fd, message.data);
            break;
        case MSG_JOIN_ROOM:
            handleJoinRoom(fd, message.data);
            break;
        case MSG_LEAVE_ROOM:
            handleLeaveRoom(fd, message.data);
            break;
        case MSG_GET_USER_INFO:
            handleGetUserInfo(fd, message.data);
            break;
        default:
            break;
    }
}

void ChatRoomServer::handleGetUserInfo(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_GET_USER_INFO_RESPONSE, "JSON格式错误");
        return;
    }

    if (!validateRequiredFields(root, {"user_id", "token"})) {
        sendErrorResponse(fd, MSG_GET_USER_INFO_RESPONSE, "缺少必需参数");
        return;
    }

    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_GET_USER_INFO_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    }

    int userId = root["user_id"].asInt();
    auto serviceResult = service_manager_->getUserInfo(userId);
    if (!serviceResult.ok) {
        sendErrorResponse(fd, MSG_GET_USER_INFO_RESPONSE, "获取用户信息失败");
        return;
    }

    Json::Value response;
    response["success"] = serviceResult.ok;
    response["code"] = static_cast<int>(serviceResult.code);
    response["user_info"] = Json::Value(Json::objectValue);
    response["user_info"]["discriminator"] = serviceResult.data.discriminator;
    response["user_info"]["name"] = serviceResult.data.name;
    response["user_info"]["email"] = serviceResult.data.email;
    response["user_info"]["created_time"] = serviceResult.data.created_time;

    sendResponse(fd, MSG_GET_USER_INFO_RESPONSE, response);
}

void ChatRoomServer::handleRegister(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_REGISTER_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"email", "password", "name"})) {
        sendErrorResponse(fd, MSG_REGISTER_RESPONSE, "缺少必需参数");
        return;
    }
    
    auto result = service_manager_->registerUser(
        root["email"].asString(), 
        root["password"].asString(), 
        root["name"].asString()
    );
    
    Json::Value response;
    response["success"] = result.ok;
    response["message"] = result.message;
    response["code"] = static_cast<int>(result.code);
    
    sendResponse(fd, MSG_REGISTER_RESPONSE, response);
}

void ChatRoomServer::handleChangePassword(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_CHANGE_PASSWORD_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"email", "old_password", "new_password"})) {
        sendErrorResponse(fd, MSG_CHANGE_PASSWORD_RESPONSE, "缺少必需参数");
        return;
    }
    
    auto result = service_manager_->changePassword(
        root["email"].asString(),
        root["old_password"].asString(),
        root["new_password"].asString()
    );
    
    Json::Value response;
    response["success"] = result.ok;
    response["message"] = result.message;
    response["code"] = static_cast<int>(result.code);
    
    sendResponse(fd, MSG_CHANGE_PASSWORD_RESPONSE, response);
}

void ChatRoomServer::handleChangeDisplayName(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_CHANGE_DISPLAY_NAME_RESPONSE, "JSON格式错误");
        return;
    }

    if (!validateRequiredFields(root, {"display_name", "token"})) {
        sendErrorResponse(fd, MSG_CHANGE_DISPLAY_NAME_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_CHANGE_DISPLAY_NAME_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    }
    
    int userId = -1;
    {
        std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
        auto it = fd_to_userId_.find(fd);
        if (it != fd_to_userId_.end()) {
            userId = it->second;
        }
    }
    
    auto serviceResult = service_manager_->changeDisplayName(userId, root["display_name"].asString());
    
    Json::Value response;
    response["success"] = serviceResult.ok;
    response["message"] = serviceResult.message;
    response["code"] = static_cast<int>(serviceResult.code);
    
    sendResponse(fd, MSG_CHANGE_DISPLAY_NAME_RESPONSE, response);
}

void ChatRoomServer::handleLogin(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_LOGIN_RESPONSE, "JSON格式错误");
        return;
    }

    if (!validateRequiredFields(root, {"email", "password"})) {
        sendErrorResponse(fd, MSG_LOGIN_RESPONSE, "缺少必需参数");
        return;
    }
    
    auto result = service_manager_->login(
        root["email"].asString(),
        root["password"].asString()
    );
    
    Json::Value response;
    response["success"] = result.ok;
    response["message"] = result.message;
    response["code"] = static_cast<int>(result.code);
    
    if (result.ok) {
        int userId = result.data.id;
        
        int oldFd = -1;
        {
            std::lock_guard<std::mutex> lock(userId_to_fd_mutex_);
            auto oldFdIt = userId_to_fd_.find(userId);
            if (oldFdIt != userId_to_fd_.end()) {
                oldFd = oldFdIt->second;
                if (oldFd == fd) {
                    oldFd = -1;
                }
            }
        }
        
        if (oldFd != -1) {
            uint16_t kickMsg = htons(MSG_ACCOUNT_KICKED);
            constexpr int maxRetries = 10;
            constexpr int retryDelayMs = 10;
            
            for (int i = 0; i < maxRetries; ++i) {
                ssize_t sent = ::send(oldFd, &kickMsg, sizeof(kickMsg), 0);
                
                if (sent == sizeof(kickMsg)) {
                    break;
                }
                
                if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
                } else {
                    break;
                }
            }
            
            cleanupConnection(oldFd);
        }
        
        {
            std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
            fd_to_userId_[fd] = userId;
        }
        
        {
            std::lock_guard<std::mutex> lock(userId_to_fd_mutex_);
            userId_to_fd_[userId] = fd;
        }
        
        bool isAdmin = result.data.is_admin;
        std::string token = generateToken(userId, isAdmin ? "a" : "u");
        response["token"] = token;
        
        response["user"] = Json::Value();
        response["user"]["id"] = result.data.id;
        response["user"]["discriminator"] = result.data.discriminator;
        response["user"]["name"] = result.data.name;
        response["user"]["email"] = result.data.email;
        response["user"]["is_admin"] = isAdmin;
        response["user"]["created_time"] = result.data.created_time;
        

        response["active_rooms"] = Json::Value(Json::arrayValue);
        {
            std::lock_guard<std::mutex> lock(active_rooms_mutex_);
            for (const auto& pair : active_rooms_) {
                Json::Value roomInfo;
                roomInfo["id"] = pair.first;
                roomInfo["name"] = pair.second.name;
                roomInfo["description"] = pair.second.description;
                roomInfo["creator_id"] = pair.second.creator_id;
                roomInfo["max_users"] = pair.second.max_users;
                roomInfo["current_users"] = pair.second.users.size();
                roomInfo["created_time"] = pair.second.created_time;
                response["active_rooms"].append(roomInfo);
            }
        }
        
        if (isAdmin) {
            response["inactive_rooms"] = Json::Value(Json::arrayValue);
            {
                std::lock_guard<std::mutex> lock(inactive_rooms_mutex_);
                for (const auto& pair : inactive_rooms_) {
                    Json::Value roomInfo;
                    roomInfo["id"] = pair.first;
                    roomInfo["name"] = pair.second.name;
                    roomInfo["description"] = pair.second.description;
                    roomInfo["creator_id"] = pair.second.creator_id;
                    roomInfo["max_users"] = pair.second.max_users;
                    roomInfo["current_users"] = pair.second.users.size();
                    roomInfo["created_time"] = pair.second.created_time;
                    response["inactive_rooms"].append(roomInfo);
                }
            }
        }
    }
    
    sendResponse(fd, MSG_LOGIN_RESPONSE, response);
}

void ChatRoomServer::handleLogout(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_LOGOUT_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"token"})) {
        sendErrorResponse(fd, MSG_LOGOUT_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_LOGOUT_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    }
    
    cleanupConnection(fd);
}

void ChatRoomServer::handleFetchActiveRooms(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_FETCH_ACTIVE_ROOMS_RESPONSE, "JSON格式错误");
        return;
    }

    if (!validateRequiredFields(root, {"token"})) {
        sendErrorResponse(fd, MSG_FETCH_ACTIVE_ROOMS_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_FETCH_ACTIVE_ROOMS_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    }
    
    Json::Value response;
    response["type"] = MSG_FETCH_ACTIVE_ROOMS_RESPONSE;
    response["success"] = true;
    response["rooms"] = Json::Value(Json::arrayValue);

    {
        std::lock_guard<std::mutex> lock(active_rooms_mutex_);
        for (const auto& pair : active_rooms_) {
            Json::Value roomInfo;
            roomInfo["id"] = pair.first;
            roomInfo["name"] = pair.second.name;
            roomInfo["description"] = pair.second.description;
            roomInfo["creator_id"] = pair.second.creator_id;
            roomInfo["max_users"] = pair.second.max_users;
            roomInfo["current_users"] = pair.second.users.size();
            roomInfo["created_time"] = pair.second.created_time;
            response["rooms"].append(roomInfo);
        }
    }

    sendResponse(fd, MSG_FETCH_ACTIVE_ROOMS_RESPONSE, response);
}

void ChatRoomServer::handleFetchInactiveRooms(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_FETCH_INACTIVE_ROOMS_RESPONSE, "JSON格式错误");
        return;
    }

    if (!validateRequiredFields(root, {"token"})) {
        sendErrorResponse(fd, MSG_FETCH_INACTIVE_ROOMS_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_FETCH_INACTIVE_ROOMS_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    } else if (result == 0) {
        sendErrorResponse(fd, MSG_FETCH_INACTIVE_ROOMS_RESPONSE, "需要管理员权限");
        return;
    }
    
    Json::Value response;
    response["type"] = MSG_FETCH_INACTIVE_ROOMS_RESPONSE;
    response["success"] = true;
    response["rooms"] = Json::Value(Json::arrayValue);

    {
        std::lock_guard<std::mutex> lock(inactive_rooms_mutex_);
        for (const auto& pair : inactive_rooms_) {
            Json::Value roomInfo;
            roomInfo["id"] = pair.first;
            roomInfo["name"] = pair.second.name;
            roomInfo["description"] = pair.second.description;
            roomInfo["creator_id"] = pair.second.creator_id;
            roomInfo["max_users"] = pair.second.max_users;
            roomInfo["current_users"] = pair.second.users.size();
            roomInfo["created_time"] = pair.second.created_time;
            response["rooms"].append(roomInfo);
        }
    }

    sendResponse(fd, MSG_FETCH_INACTIVE_ROOMS_RESPONSE, response);
}

void ChatRoomServer::handleCreateRoom(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_CREATE_ROOM_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"name", "description", "max_users", "token"})) {
        sendErrorResponse(fd, MSG_CREATE_ROOM_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_CREATE_ROOM_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    } else if (result == 0) {
        sendErrorResponse(fd, MSG_CREATE_ROOM_RESPONSE, "需要管理员权限");
        return;
    }
    
    int userId = -1;
    {
        std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
        auto it = fd_to_userId_.find(fd);
        if (it != fd_to_userId_.end()) {
            userId = it->second;
        }
    }

    auto serviceResult = service_manager_->createRoom(
        userId,
        root["name"].asString(),
        root["description"].asString(),
        root["max_users"].asInt()
    );
    
    Json::Value response;
    response["type"] = MSG_CREATE_ROOM_RESPONSE;
    response["success"] = serviceResult.ok;
    if (serviceResult.ok) {
        RoomInfo roomInfo(
            root["name"].asString(),
            root["description"].asString(),
            root["max_users"].asInt(),
            userId,
            serviceResult.data.created_time
        );
        
        {
            std::lock_guard<std::mutex> lock(active_rooms_mutex_);
            active_rooms_[serviceResult.data.id] = roomInfo;
        }
    } else {
        response["message"] = serviceResult.message;
    }
    
    sendResponse(fd, MSG_CREATE_ROOM_RESPONSE, response);
}

void ChatRoomServer::handleDeleteRoom(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_DELETE_ROOM_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"room_id", "token"})) {
        sendErrorResponse(fd, MSG_DELETE_ROOM_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_DELETE_ROOM_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    } else if (result == 0) {
        sendErrorResponse(fd, MSG_DELETE_ROOM_RESPONSE, "需要管理员权限");
        return;
    }
    
    int roomId = root["room_id"].asInt();
    auto serviceResult = service_manager_->deleteRoom(roomId);
    
    Json::Value response;
    response["type"] = MSG_DELETE_ROOM_RESPONSE;
    response["success"] = serviceResult.ok;
    if (serviceResult.ok) {
        {
            std::lock_guard<std::mutex> lock(active_rooms_mutex_);
            active_rooms_.erase(roomId);
        }
        {
            std::lock_guard<std::mutex> lock(inactive_rooms_mutex_);
            inactive_rooms_.erase(roomId);
        }
    } else {
        response["message"] = serviceResult.message;
    }
    
    sendResponse(fd, MSG_DELETE_ROOM_RESPONSE, response);
}

void ChatRoomServer::handleSetRoomName(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_SET_ROOM_NAME_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"room_id", "name", "token"})) {
        sendErrorResponse(fd, MSG_SET_ROOM_NAME_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_SET_ROOM_NAME_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    } else if (result == 0) {
        sendErrorResponse(fd, MSG_SET_ROOM_NAME_RESPONSE, "需要管理员权限");
        return;
    }

    int roomId = root["room_id"].asInt();
    std::string name = root["name"].asString();
    auto serviceResult = service_manager_->setRoomName(roomId, name);

    Json::Value response;
    response["type"] = MSG_SET_ROOM_NAME_RESPONSE;
    response["success"] = serviceResult.ok;
    if (serviceResult.ok) {        
        bool roomFound = false;
        {
            std::lock_guard<std::mutex> activeRoomsLock(active_rooms_mutex_);
            auto activeIt = active_rooms_.find(roomId);
            if (activeIt != active_rooms_.end()) {
                activeIt->second.name = name;
                roomFound = true;
            }
        }
        if (!roomFound) {
            std::lock_guard<std::mutex> inactiveRoomsLock(inactive_rooms_mutex_);
            auto inactiveIt = inactive_rooms_.find(roomId);
            if (inactiveIt != inactive_rooms_.end()) {
                inactiveIt->second.name = name;
            }
        }
        
        if (roomFound) {
            Json::Value notification;
            notification["room_id"] = roomId;
            notification["room_name"] = name;
            notifyRoomUsers(roomId, MSG_ROOM_NAME_UPDATE_PUSH, notification);
        }
    } else {
        response["message"] = serviceResult.message;
    }
    
    sendResponse(fd, MSG_SET_ROOM_NAME_RESPONSE, response);
}

void ChatRoomServer::handleSetRoomDescription(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_SET_ROOM_DESCRIPTION_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"room_id", "description", "token"})) {
        sendErrorResponse(fd, MSG_SET_ROOM_DESCRIPTION_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_SET_ROOM_DESCRIPTION_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    } else if (result == 0) {
        sendErrorResponse(fd, MSG_SET_ROOM_DESCRIPTION_RESPONSE, "需要管理员权限");
        return;
    }

    int roomId = root["room_id"].asInt();
    std::string description = root["description"].asString();
    auto serviceResult = service_manager_->setRoomDescription(roomId, description);

    Json::Value response;
    response["type"] = MSG_SET_ROOM_DESCRIPTION_RESPONSE;
    response["success"] = serviceResult.ok;
    if (serviceResult.ok) {        
        bool roomFound = false;
        {
            std::lock_guard<std::mutex> activeRoomsLock(active_rooms_mutex_);
            auto activeIt = active_rooms_.find(roomId);
            if (activeIt != active_rooms_.end()) {
                activeIt->second.description = description;
                roomFound = true;
            }
        }
        if (!roomFound) {
            std::lock_guard<std::mutex> inactiveRoomsLock(inactive_rooms_mutex_);
            auto inactiveIt = inactive_rooms_.find(roomId);
            if (inactiveIt != inactive_rooms_.end()) {
                inactiveIt->second.description = description;
            }
        }
        
        if (roomFound) {
            Json::Value notification;
            notification["room_id"] = roomId;
            notification["room_description"] = description;
            notifyRoomUsers(roomId, MSG_ROOM_DESCRIPTION_UPDATE_PUSH, notification);
        }
    } else {
        response["message"] = serviceResult.message;
    }

    sendResponse(fd, MSG_SET_ROOM_DESCRIPTION_RESPONSE, response);
}

void ChatRoomServer::handleSetRoomMaxUsers(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_SET_ROOM_MAX_USERS_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"room_id", "max_users", "token"})) {
        sendErrorResponse(fd, MSG_SET_ROOM_MAX_USERS_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_SET_ROOM_MAX_USERS_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    } else if (result == 0) {
        sendErrorResponse(fd, MSG_SET_ROOM_MAX_USERS_RESPONSE, "需要管理员权限");
        return;
    }

    int roomId = root["room_id"].asInt();
    int maxUsers = root["max_users"].asInt();
    auto serviceResult = service_manager_->setRoomMaxUsers(roomId, maxUsers);

    Json::Value response;
    response["type"] = MSG_SET_ROOM_MAX_USERS_RESPONSE;
    response["success"] = serviceResult.ok;
    if (serviceResult.ok) {
        bool roomFound = false;
        {
            std::lock_guard<std::mutex> activeRoomsLock(active_rooms_mutex_);
            auto activeIt = active_rooms_.find(roomId);
            if (activeIt != active_rooms_.end()) {
                activeIt->second.max_users = maxUsers;
                roomFound = true;
            }
        }
        if (!roomFound) {
            std::lock_guard<std::mutex> inactiveRoomsLock(inactive_rooms_mutex_);
            auto inactiveIt = inactive_rooms_.find(roomId);
            if (inactiveIt != inactive_rooms_.end()) {
                inactiveIt->second.max_users = maxUsers;
            }
        }
        
        if (roomFound) {
            Json::Value notification;
            notification["room_id"] = roomId;
            notification["room_max_users"] = maxUsers;
            notifyRoomUsers(roomId, MSG_ROOM_MAX_USERS_UPDATE_PUSH, notification);
        }
    } else {
        response["message"] = serviceResult.message;
    }

    sendResponse(fd, MSG_SET_ROOM_MAX_USERS_RESPONSE, response);
}

void ChatRoomServer::handleSetRoomStatus(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_SET_ROOM_STATUS_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"room_id", "status", "token"})) {
        sendErrorResponse(fd, MSG_SET_ROOM_STATUS_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);  
    if (result == 2) {
        sendErrorResponse(fd, MSG_SET_ROOM_STATUS_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    } else if (result == 0) {
        sendErrorResponse(fd, MSG_SET_ROOM_STATUS_RESPONSE, "需要管理员权限");
        return;
    }   

    int roomId = root["room_id"].asInt();
    int status = root["status"].asInt();
    auto serviceResult = service_manager_->setRoomStatus(roomId, status);

    Json::Value response;   
    response["type"] = MSG_SET_ROOM_STATUS_RESPONSE;
    response["success"] = serviceResult.ok;
    if (serviceResult.ok) {
        if (status == 1) {
            std::lock_guard<std::mutex> activeRoomsLock(active_rooms_mutex_);
            std::lock_guard<std::mutex> inactiveRoomsLock(inactive_rooms_mutex_);
            
            auto inactiveIt = inactive_rooms_.find(roomId);
            if (inactiveIt != inactive_rooms_.end()) {
                active_rooms_[roomId] = inactiveIt->second;
                inactive_rooms_.erase(inactiveIt);
            }
        } else {
            std::lock_guard<std::mutex> activeRoomsLock(active_rooms_mutex_);
            auto activeIt = active_rooms_.find(roomId);
            if (activeIt != active_rooms_.end()) {
                std::vector<int> usersToRemove;
                usersToRemove.assign(activeIt->second.users.begin(), activeIt->second.users.end());
                activeIt->second.users.clear();
                
                {
                    std::lock_guard<std::mutex> userIdToRoomLock(userId_to_roomId_mutex_);
                    for (int userId : usersToRemove) {
                        userId_to_roomId_.erase(userId);
                    }
                }
                
                RoomInfo roomToMove = activeIt->second;
                {
                    std::lock_guard<std::mutex> inactiveRoomsLock(inactive_rooms_mutex_);
                    inactive_rooms_[roomId] = std::move(roomToMove);
                }
                active_rooms_.erase(activeIt);
            }
        }
    } else {    
        response["message"] = serviceResult.message;
    }

    sendResponse(fd, MSG_SET_ROOM_STATUS_RESPONSE, response);
}

void ChatRoomServer::handleSendMessage(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_SEND_MESSAGE_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"message", "token"})) {
        sendErrorResponse(fd, MSG_SEND_MESSAGE_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_SEND_MESSAGE_RESPONSE, "Token无效或已过期");
        return;
    }

    std::string message = root["message"].asString();
    if (message.empty()) {
        sendErrorResponse(fd, MSG_SEND_MESSAGE_RESPONSE, "消息不能为空");
        return;
    }

    int userId = -1;
    {
        std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
        auto it = fd_to_userId_.find(fd);
        if (it == fd_to_userId_.end()) {
            sendErrorResponse(fd, MSG_SEND_MESSAGE_RESPONSE, "用户未登录");
            return;
        }
        userId = it->second;
    }
    
    int roomId = -1;
    {
        std::lock_guard<std::mutex> lock(userId_to_roomId_mutex_);
        auto it = userId_to_roomId_.find(userId);
        if (it == userId_to_roomId_.end()) {
            sendErrorResponse(fd, MSG_SEND_MESSAGE_RESPONSE, "您当前不在任何房间中");
            return;
        }
        roomId = it->second;
    }
    
    auto userResult = service_manager_->getUserInfo(userId);
    if (!userResult.ok) {
        sendErrorResponse(fd, MSG_SEND_MESSAGE_RESPONSE, "获取用户信息失败");
        return;
    }
    
    std::string display_name = userResult.data.name + "#" + userResult.data.discriminator;
    auto serviceResult = service_manager_->sendMessage(userId, roomId, message, display_name, TimeUtils::getCurrentTimeString());
    if (!serviceResult.ok) {
        sendErrorResponse(fd, MSG_SEND_MESSAGE_RESPONSE, "消息发送失败");
        return;
    }

    Json::Value notification;
    notification["display_name"] = display_name;
    notification["message"] = message;
    notification["timestamp"] = TimeUtils::getCurrentTimestamp();
    notifyRoomUsers(roomId, MSG_CHAT_MESSAGE_PUSH, notification);

    Json::Value response;
    response["type"] = MSG_SEND_MESSAGE_RESPONSE;
    response["success"] = serviceResult.ok;

    sendResponse(fd, MSG_SEND_MESSAGE_RESPONSE, response);
}

// 函数有待优化 按理来说是像qq那样子无限网上拉取消息 那就还需要一个参数来表示位置
void ChatRoomServer::handleGetMessageHistory(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_GET_MESSAGE_HISTORY_RESPONSE, "JSON格式错误");
        return;
    }

    if (!validateRequiredFields(root, {"token"})) {
        sendErrorResponse(fd, MSG_GET_MESSAGE_HISTORY_RESPONSE, "缺少必需参数");
        return;
    }

    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_GET_MESSAGE_HISTORY_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    }

    int userId = -1;
    {
        std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
        auto it = fd_to_userId_.find(fd);
        if (it == fd_to_userId_.end()) {
            sendErrorResponse(fd, MSG_GET_MESSAGE_HISTORY_RESPONSE, "用户未登录");
            return;
        }
        userId = it->second;
    }

    int roomId = -1;
    {
        std::lock_guard<std::mutex> lock(userId_to_roomId_mutex_);
        auto it = userId_to_roomId_.find(userId);
        if (it == userId_to_roomId_.end()) {
            sendErrorResponse(fd, MSG_GET_MESSAGE_HISTORY_RESPONSE, "您当前不在任何房间中");
            return;
        }
        roomId = it->second;
    }

    auto serviceResult = service_manager_->getMessageHistory(roomId, 50);

    Json::Value response;
    response["type"] = MSG_GET_MESSAGE_HISTORY_RESPONSE;
    response["success"] = serviceResult.ok;
    if (serviceResult.ok) {
        response["message_history"] = Json::Value(Json::arrayValue);
        for (const auto& msg : serviceResult.data) {
            Json::Value messageObj;
            messageObj["message_id"] = msg.message_id;
            messageObj["user_id"] = msg.user_id;
            messageObj["room_id"] = msg.room_id;
            messageObj["content"] = msg.content;
            messageObj["display_name"] = msg.display_name;
            messageObj["send_time"] = msg.send_time;
            response["message_history"].append(messageObj);
        }
    } else {
        response["message"] = serviceResult.message;
    }

    sendResponse(fd, MSG_GET_MESSAGE_HISTORY_RESPONSE, response);
}

void ChatRoomServer::handleJoinRoom(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_JOIN_ROOM_RESPONSE, "JSON格式错误");
        return;
    }

    if (!validateRequiredFields(root, {"room_id", "token"})) {
        sendErrorResponse(fd, MSG_JOIN_ROOM_RESPONSE, "缺少必需参数");
        return;
    }

    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_JOIN_ROOM_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    }

    int userId = -1;
    {
        std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
        auto it = fd_to_userId_.find(fd);
        if (it == fd_to_userId_.end()) {
            sendErrorResponse(fd, MSG_JOIN_ROOM_RESPONSE, "用户未登录");
            return;
        }
        userId = it->second;
    }

    int roomId = root["room_id"].asInt();
    {   
        std::lock_guard<std::mutex> activeRoomsLock(active_rooms_mutex_);
        std::lock_guard<std::mutex> userIdToRoomLock(userId_to_roomId_mutex_);
        
        auto userIt = userId_to_roomId_.find(userId);
        if (userIt != userId_to_roomId_.end()) {
            sendErrorResponse(fd, MSG_JOIN_ROOM_RESPONSE, "您已经在房间中");
            return;
        }
        
        auto it = active_rooms_.find(roomId);
        if (it == active_rooms_.end()) {
            sendErrorResponse(fd, MSG_JOIN_ROOM_RESPONSE, "房间不存在");
            return;
        }
        auto& room = it->second;
        if (room.users.size() >= static_cast<size_t>(room.max_users)) {
            sendErrorResponse(fd, MSG_JOIN_ROOM_RESPONSE, "房间人数已满");
            return;
        }
        room.users.insert(userId);
        userId_to_roomId_[userId] = roomId;
    }

    Json::Value notification;
    notification["user_id"] = userId;
    notification["room_id"] = roomId;
    notifyRoomUsers(roomId, MSG_USER_JOIN_PUSH, notification);

    Json::Value response;
    response["type"] = MSG_JOIN_ROOM_RESPONSE;
    response["success"] = true;
    sendResponse(fd, MSG_JOIN_ROOM_RESPONSE, response);
}

void ChatRoomServer::handleLeaveRoom(int fd, const std::string& data) {
    Json::Value root;
    if (!parseJson(data, root)) {
        sendErrorResponse(fd, MSG_LEAVE_ROOM_RESPONSE, "JSON格式错误");
        return;
    }
    
    if (!validateRequiredFields(root, {"token"})) {
        sendErrorResponse(fd, MSG_LEAVE_ROOM_RESPONSE, "缺少必需参数");
        return;
    }
    
    std::string token = root["token"].asString();
    int result = validateToken(fd, token);
    if (result == 2) {
        sendErrorResponse(fd, MSG_LEAVE_ROOM_RESPONSE, "Token无效或已过期, 请重新登录");
        return;
    }
    
    int userId = -1;
    {
        std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
        auto it = fd_to_userId_.find(fd);
        if (it == fd_to_userId_.end()) {
            sendErrorResponse(fd, MSG_LEAVE_ROOM_RESPONSE, "用户未登录");
            return;
        }
        userId = it->second;
    }
    
    int roomId = -1;
    {   
        std::lock_guard<std::mutex> activeRoomsLock(active_rooms_mutex_);
        std::lock_guard<std::mutex> userIdToRoomLock(userId_to_roomId_mutex_);
        
        auto userIt = userId_to_roomId_.find(userId);
        if (userIt == userId_to_roomId_.end()) {
            sendErrorResponse(fd, MSG_LEAVE_ROOM_RESPONSE, "您当前不在任何房间中");
            return;
        }
        
        roomId = userIt->second;
        auto it = active_rooms_.find(roomId);
        if (it == active_rooms_.end()) {
            sendErrorResponse(fd, MSG_LEAVE_ROOM_RESPONSE, "房间不存在");
            return;
        }
        
        auto& room = it->second;
        room.users.erase(userId);
        userId_to_roomId_.erase(userIt);
    }

    Json::Value notification;
    notification["user_id"] = userId;
    notification["room_id"] = roomId;
    notifyRoomUsers(roomId, MSG_USER_LEAVE_PUSH, notification);

    Json::Value response;
    response["type"] = MSG_LEAVE_ROOM_RESPONSE;
    response["success"] = true;
    sendResponse(fd, MSG_LEAVE_ROOM_RESPONSE, response);
}

void ChatRoomServer::notifyRoomUsers(int roomId, uint16_t messageType, const Json::Value& notification) {
    std::vector<int> userIdsToNotify;
    
    {
        std::lock_guard<std::mutex> activeRoomsLock(active_rooms_mutex_);
        auto it = active_rooms_.find(roomId);
        if (it != active_rooms_.end()) {
            auto& room = it->second;
            
            for (int userId : room.users) {
                userIdsToNotify.push_back(userId);
            }
        }
    }
    
    std::vector<int> fdsToNotify;
    {
        std::lock_guard<std::mutex> fdLock(userId_to_fd_mutex_);
        for (int userId : userIdsToNotify) {
            auto fdIt = userId_to_fd_.find(userId);
            if (fdIt != userId_to_fd_.end()) {
                fdsToNotify.push_back(fdIt->second);
            }
        }
    }
    
    for (int fd : fdsToNotify) {
        sendResponse(fd, messageType, notification);
    }
}

bool ChatRoomServer::parseJson(const std::string& data, Json::Value& root) {
    Json::Reader reader;
    return reader.parse(data, root);
}

bool ChatRoomServer::validateRequiredFields(const Json::Value& root, const std::vector<std::string>& requiredFields) {
    for (const auto& field : requiredFields) {
        if (!root.isMember(field)) {
            return false;
        }
    }
    return true;
}

void ChatRoomServer::sendResponse(int fd, uint16_t responseType, const Json::Value& response) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        it->second->sendMessage(responseType, response.toStyledString());
    }
}

void ChatRoomServer::sendErrorResponse(int fd, uint16_t responseType, const std::string& message) {
    Json::Value response;
    response["success"] = false;
    response["message"] = message;
    response["code"] = 400;
    sendResponse(fd, responseType, response);
}

std::string ChatRoomServer::generateToken(int userId, const std::string& role) {
    auto timestamp = TimeUtils::getCurrentTimestamp();
    
    std::string roleChar = (role == "admin") ? "a" : "n";
    int counter = token_counter_.fetch_add(1) % 10000;
    std::string token = roleChar + "_" + 
                       std::to_string(timestamp) + "_" +
                       std::to_string(counter);
    
    int64_t expireTime = timestamp + (token_expire_minutes_ * 60 * 1000);
    
    {
        std::lock_guard<std::mutex> lock(userId_to_token_mutex_);
        userId_to_token_[userId] = std::make_pair(token, expireTime);
    }
    
    return token;
}

int ChatRoomServer::validateToken(int fd, std::string& token) {
    int userId = -1;
    {
        std::lock_guard<std::mutex> lock(fd_to_userId_mutex_);
        auto it = fd_to_userId_.find(fd);
        if (it != fd_to_userId_.end()) {
            userId = it->second;
        }
    }

    if (userId == -1) {
        return 2;
    }

    auto currentTime = TimeUtils::getCurrentTimestamp();

    {
        std::lock_guard<std::mutex> lock(userId_to_token_mutex_);
        auto it = userId_to_token_.find(userId);
        if (it == userId_to_token_.end()) {
            return 2;
        }
        
        if (currentTime > it->second.second) {
            return 2;
        }
        
        if (it->second.first != token) {
            return 2;
        }
        
        if (token[0] == 'a') {
            return 1;
        } else if (token[0] == 'n') {
            return 0;
        }
    }

    return 2;
}

// 本质上清理线程也应该分区域 但考虑到量级不大 就先这样了
void ChatRoomServer::cleanupExpiredTokens() {
    auto currentTime = TimeUtils::getCurrentTimestamp();
    
    std::lock_guard<std::mutex> lock(userId_to_token_mutex_);
    
    for (auto it = userId_to_token_.begin(); it != userId_to_token_.end();) {
        if (currentTime > it->second.second) {
            it = userId_to_token_.erase(it);
        } else {
            ++it;
        }
    }
}