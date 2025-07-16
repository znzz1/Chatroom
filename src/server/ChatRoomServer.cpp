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
    
    setNonBlocking(listen_fd_);
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
                rooms_[room.id] = roomInfo;
            }
        }
        std::cout << "Loaded " << (rooms_.size() + active_rooms_.size())
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

void ChatRoomServer::handleReadEvent(int fd) {
    thread_pool_.addTask([this, fd]() {
        this->processReadTask(fd);
    });
}

void ChatRoomServer::handleWriteEvent(int fd) {
    thread_pool_.addTask([this, fd]() {
        this->processWriteTask(fd);
    });
}

void ChatRoomServer::processReadTask(int fd) {
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

void ChatRoomServer::processWriteTask(int fd) {
    trySendData(fd);
}

void ChatRoomServer::processMessage(int fd, const Message& message) {
    // 根据消息类型分类处理
    switch (message.type) {
        // API 请求 - 通过 Service 层处理
        case MSG_LOGIN:
        case MSG_REGISTER:
        case MSG_CHANGE_PASSWORD:
        case MSG_CHANGE_DISPLAY_NAME:
        case MSG_GET_ACTIVE_ROOMS:
        case MSG_GET_ROOM_INFO:
        case MSG_CREATE_ROOM:
        case MSG_DELETE_ROOM:
        case MSG_SET_ROOM_NAME:
        case MSG_SET_ROOM_DESCRIPTION:
        case MSG_SET_ROOM_MAX_USERS:
        case MSG_SET_ROOM_STATUS:
        case MSG_GET_ALL_ROOMS:
        case MSG_GET_MESSAGE_HISTORY:
            handleApiRequest(fd, message);
            break;
            
        // 实时消息 - 直接内存操作
        case MSG_SEND_MESSAGE:
            handleRealTimeMessage(fd, message);
            break;
            
        default:
            std::cerr << "Unknown message type: " << message.type << std::endl;
            break;
    }
}

void ChatRoomServer::handleApiRequest(int fd, const Message& message) {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(message.data, root)) {
        sendErrorResponse(fd, "Invalid JSON format", 400);
        return;
    }
    
    switch (message.type) {
        case MSG_LOGIN:
            handleLogin(fd, root);
            break;
        case MSG_REGISTER:
            handleRegister(fd, root);
            break;
        case MSG_GET_ACTIVE_ROOMS:
            handleGetActiveRooms(fd);
            break;
        case MSG_GET_ROOM_INFO:
            handleGetRoomInfo(fd, root);
            break;
        case MSG_CREATE_ROOM:
            handleCreateRoom(fd, root);
            break;
        case MSG_GET_MESSAGE_HISTORY:
            handleGetMessageHistory(fd, root);
            break;
        default:
            sendErrorResponse(fd, "Unsupported API request", 400);
            break;
    }
}

void ChatRoomServer::handleLogin(int fd, const Json::Value& root) {
    if (!root.isMember("email") || !root.isMember("password")) {
        sendErrorResponse(fd, "Missing email or password", 400);
        return;
    }
    
    std::string email = root["email"].asString();
    std::string password = root["password"].asString();
    
    auto result = service_manager_->login(email, password);
    
    Json::Value response = buildResponse("login_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        response["data"]["user_id"] = result.data.id;
        response["data"]["display_name"] = result.data.getFullName();
        response["data"]["role"] = static_cast<int>(result.data.role);
        
        // 将用户添加到在线用户列表
        auto conn = connections_.find(fd);
        if (conn != connections_.end()) {
            addOnlineUser(result.data.id, result.data.getFullName(), 0, conn->second);
        }
    }
    
    sendResponse(fd, MSG_LOGIN_RESPONSE, response);
}

void ChatRoomServer::handleRegister(int fd, const Json::Value& root) {
    if (!root.isMember("email") || !root.isMember("password") || !root.isMember("name")) {
        sendErrorResponse(fd, "Missing required fields", 400);
        return;
    }
    
    std::string email = root["email"].asString();
    std::string password = root["password"].asString();
    std::string name = root["name"].asString();
    
    auto result = service_manager_->registerUser(email, password, name);
    
    Json::Value response = buildResponse("register_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        response["data"]["user_id"] = result.data.id;
        response["data"]["display_name"] = result.data.getFullName();
    }
    
    sendResponse(fd, MSG_REGISTER_RESPONSE, response);
}

void ChatRoomServer::handleGetActiveRooms(int fd) {
    auto result = service_manager_->getActiveRooms();
    
    Json::Value response = buildResponse("get_active_rooms_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        Json::Value roomsArray;
        for (const auto& room : result.data) {
            Json::Value roomObj;
            roomObj["room_id"] = room.id;
            roomObj["name"] = room.name;
            roomObj["description"] = room.description;
            roomObj["creator_id"] = room.creator_id;
            roomObj["max_users"] = room.max_users;
            roomObj["status"] = (room.status == RoomStatus::ACTIVE) ? "ACTIVE" : "INACTIVE";
            roomObj["created_time"] = room.created_time;
            
            // 添加在线用户数
            std::lock_guard<std::mutex> lock(rooms_mutex_);
            auto roomIt = rooms_.find(room.id);
            if (roomIt != rooms_.end()) {
                roomObj["online_users"] = static_cast<int>(roomIt->second.onlineUsers.size());
            } else {
                roomObj["online_users"] = 0;
            }
            
            roomsArray.append(roomObj);
        }
        response["data"]["rooms"] = roomsArray;
    }
    
    sendResponse(fd, MSG_GET_ACTIVE_ROOMS_RESPONSE, response);
}

void ChatRoomServer::handleGetRoomInfo(int fd, const Json::Value& root) {
    if (!root.isMember("room_id")) {
        sendErrorResponse(fd, "Missing room_id", 400);
        return;
    }
    
    int roomId = root["room_id"].asInt();
    auto result = service_manager_->getRoomInfo(roomId);
    
    Json::Value response = buildResponse("get_room_info_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        Json::Value roomObj;
        roomObj["room_id"] = result.data.id;
        roomObj["name"] = result.data.name;
        roomObj["description"] = result.data.description;
        roomObj["creator_id"] = result.data.creator_id;
        roomObj["max_users"] = result.data.max_users;
        roomObj["status"] = (result.data.status == RoomStatus::ACTIVE) ? "ACTIVE" : "INACTIVE";
        roomObj["created_time"] = result.data.created_time;
        
        // 添加在线用户列表
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        auto roomIt = rooms_.find(roomId);
        if (roomIt != rooms_.end()) {
            Json::Value usersArray;
            for (int userId : roomIt->second.onlineUsers) {
                std::lock_guard<std::mutex> userLock(users_mutex_);
                auto userIt = online_users_.find(userId);
                if (userIt != online_users_.end()) {
                    Json::Value userObj;
                    userObj["user_id"] = userId;
                    userObj["display_name"] = userIt->second.displayName;
                    usersArray.append(userObj);
                }
            }
            roomObj["online_users"] = usersArray;
        }
        
        response["data"]["room"] = roomObj;
    }
    
    sendResponse(fd, MSG_GET_ROOM_INFO_RESPONSE, response);
}

void ChatRoomServer::handleCreateRoom(int fd, const Json::Value& root) {
    if (!root.isMember("admin_id") || !root.isMember("name") || !root.isMember("description")) {
        sendErrorResponse(fd, "Missing required fields", 400);
        return;
    }
    
    int adminId = root["admin_id"].asInt();
    std::string name = root["name"].asString();
    std::string description = root["description"].asString();
    int maxUsers = root.get("max_users", 0).asInt();
    
    auto result = service_manager_->createRoom(adminId, name, description, maxUsers);
    
    Json::Value response = buildResponse("create_room_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        Json::Value roomObj;
        roomObj["room_id"] = result.data.id;
        roomObj["name"] = result.data.name;
        roomObj["description"] = result.data.description;
        roomObj["creator_id"] = result.data.creator_id;
        roomObj["max_users"] = result.data.max_users;
        roomObj["status"] = (result.data.status == RoomStatus::ACTIVE) ? "ACTIVE" : "INACTIVE";
        roomObj["created_time"] = result.data.created_time;
        response["data"]["room"] = roomObj;
        
        // 添加到内存中的房间列表
        std::lock_guard<std::mutex> lock(rooms_mutex_);
        RoomInfo roomInfo;
        roomInfo.roomId = result.data.id;
        roomInfo.name = result.data.name;
        roomInfo.description = result.data.description;
        roomInfo.creatorId = result.data.creator_id;
        roomInfo.maxUsers = result.data.max_users;
        rooms_[result.data.id] = roomInfo;
    }
    
    sendResponse(fd, MSG_CREATE_ROOM_RESPONSE, response);
}

void ChatRoomServer::handleGetMessageHistory(int fd, const Json::Value& root) {
    if (!root.isMember("room_id")) {
        sendErrorResponse(fd, "Missing room_id", 400);
        return;
    }
    
    int roomId = root["room_id"].asInt();
    int limit = root.get("limit", 50).asInt();
    
    auto result = service_manager_->getMessageHistory(roomId, limit);
    
    Json::Value response = buildResponse("get_message_history_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        Json::Value messagesArray;
        for (const auto& msg : result.data) {
            Json::Value msgObj;
            msgObj["message_id"] = msg.message_id;
            msgObj["user_id"] = msg.user_id;
            msgObj["room_id"] = msg.room_id;
            msgObj["content"] = msg.content;
            msgObj["display_name"] = msg.display_name;
            msgObj["timestamp"] = msg.timestamp;
            messagesArray.append(msgObj);
        }
        response["data"]["messages"] = messagesArray;
    }
    
    sendResponse(fd, MSG_GET_MESSAGE_HISTORY_RESPONSE, response);
}

void ChatRoomServer::handleRealTimeMessage(int fd, const Message& message) {
    if (message.type == MSG_SEND_MESSAGE) {
        handleSendMessage(fd, message);
    }
}

void ChatRoomServer::handleSendMessage(int fd, const Message& message) {
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(message.data, root)) {
        return;
    }
    
    int userId = root["user_id"].asInt();
    int roomId = root["room_id"].asInt();
    std::string content = root["content"].asString();
    std::string displayName = root["display_name"].asString();
    
    // 检查用户是否在线且在指定房间
    std::lock_guard<std::mutex> userLock(users_mutex_);
    auto userIt = online_users_.find(userId);
    if (userIt == online_users_.end() || userIt->second.currentRoomId != roomId) {
        return;
    }
    
    // 广播消息给房间内所有用户
    broadcastMessageToRoom(roomId, userId, content, displayName);
    
    // 保存消息到数据库（异步）
    thread_pool_.addTask([this, userId, roomId, content, displayName]() {
        service_manager_->sendMessage(userId, roomId, content, displayName);
    });
}

void ChatRoomServer::broadcastMessageToRoom(int roomId, int senderId, const std::string& content, const std::string& displayName) {
    std::lock_guard<std::mutex> roomLock(rooms_mutex_);
    auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end()) {
        return;
    }
    
    // 构建广播消息
    Json::Value message;
    message["type"] = "chat_message";
    message["sender_id"] = senderId;
    message["sender_name"] = displayName;
    message["room_id"] = roomId;
    message["content"] = content;
    message["timestamp"] = std::to_string(time(nullptr));
    
    Json::FastWriter writer;
    std::string messageStr = writer.write(message);
    
    // 发送给房间内所有在线用户
    for (int userId : roomIt->second.onlineUsers) {
        std::lock_guard<std::mutex> userLock(users_mutex_);
        auto userIt = online_users_.find(userId);
        if (userIt != online_users_.end()) {
            auto conn = userIt->second.connection;
            if (conn && !conn->isClosed()) {
                conn->sendMessage(MSG_CHAT_MESSAGE_PUSH, messageStr);
            }
        }
    }
}

void ChatRoomServer::trySendData(int fd) {
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

void ChatRoomServer::sendResponse(int fd, uint16_t type, const Json::Value& data) {
    auto it = connections_.find(fd);
    if (it == connections_.end() || it->second->isClosed()) {
        return;
    }
    
    try {
        Json::FastWriter writer;
        it->second->sendMessage(type, writer.write(data));
    } catch (const std::exception& e) {
        std::cerr << "Error sending response: " << e.what() << std::endl;
    }
}

void ChatRoomServer::sendErrorResponse(int fd, const std::string& message, int code) {
    Json::Value response = buildResponse("error_response", false, code, message);
    sendResponse(fd, MSG_ERROR_RESPONSE, response);
}

Json::Value ChatRoomServer::buildResponse(const std::string& type, bool success, int code, const std::string& message) const {
    Json::Value response;
    response["type"] = type;
    response["success"] = success;
    response["code"] = code;
    if (!message.empty()) response["message"] = message;
    return response;
}

void ChatRoomServer::addOnlineUser(int userId, const std::string& displayName, int roomId, std::shared_ptr<Connection> conn) {
    std::lock_guard<std::mutex> lock(users_mutex_);
    OnlineUser user;
    user.userId = userId;
    user.displayName = displayName;
    user.currentRoomId = roomId;
    user.connection = conn;
    user.lastSeen = std::to_string(time(nullptr));
    online_users_[userId] = user;
    
    // 如果指定了房间，将用户添加到房间
    if (roomId > 0) {
        moveUserToRoom(userId, roomId);
    }
}

void ChatRoomServer::removeOnlineUser(int userId) {
    std::lock_guard<std::mutex> lock(users_mutex_);
    auto userIt = online_users_.find(userId);
    if (userIt != online_users_.end()) {
        int roomId = userIt->second.currentRoomId;
        online_users_.erase(userIt);
        
        // 从房间中移除用户
        if (roomId > 0) {
            std::lock_guard<std::mutex> roomLock(rooms_mutex_);
            auto roomIt = rooms_.find(roomId);
            if (roomIt != rooms_.end()) {
                roomIt->second.onlineUsers.erase(userId);
            }
        }
    }
}

void ChatRoomServer::moveUserToRoom(int userId, int newRoomId) {
    std::lock_guard<std::mutex> userLock(users_mutex_);
    auto userIt = online_users_.find(userId);
    if (userIt == online_users_.end()) {
        return;
    }
    
    int oldRoomId = userIt->second.currentRoomId;
    
    // 从旧房间移除
    if (oldRoomId > 0) {
        std::lock_guard<std::mutex> roomLock(rooms_mutex_);
        auto oldRoomIt = rooms_.find(oldRoomId);
        if (oldRoomIt != rooms_.end()) {
            oldRoomIt->second.onlineUsers.erase(userId);
        }
    }
    
    // 添加到新房间
    if (newRoomId > 0) {
        std::lock_guard<std::mutex> roomLock(rooms_mutex_);
        auto newRoomIt = rooms_.find(newRoomId);
        if (newRoomIt != rooms_.end()) {
            newRoomIt->second.onlineUsers.insert(userId);
        }
    }
    
    userIt->second.currentRoomId = newRoomId;
}

void ChatRoomServer::handleConnectionError(int fd) {
    std::cerr << "Connection error on fd " << fd << std::endl;
    handleConnectionClose(fd);
}

void ChatRoomServer::handleConnectionClose(int fd) {
    // 从在线用户列表中移除
    {
        std::lock_guard<std::mutex> userLock(users_mutex_);
        for (auto it = online_users_.begin(); it != online_users_.end(); ++it) {
            if (it->second.connection && it->second.connection->getFd() == fd) {
                int userId = it->first;
                int roomId = it->second.currentRoomId;
                
                // 从房间中移除用户
                {
                    std::lock_guard<std::mutex> roomLock(rooms_mutex_);
                    auto roomIt = rooms_.find(roomId);
                    if (roomIt != rooms_.end()) {
                        roomIt->second.onlineUsers.erase(userId);
                    }
                }
                
                online_users_.erase(it);
                break;
            }
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.erase(fd);
    }
    
    poller_.removeFd(fd);
    close(fd);
    std::cout << "Client disconnected (fd: " << fd << ")" << std::endl;
} 