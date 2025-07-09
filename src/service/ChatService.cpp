#include "service/ChatService.h"
#include "dao/UserDao.h"
#include "dao/RoomDao.h"
#include "utils/PasswordHasher.h"
#include "protocol/MessageType.h"
#include <jsoncpp/json/json.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>
#include <ctime>

ChatService::ChatService(std::unique_ptr<UserDao> user_dao, std::unique_ptr<RoomDao> room_dao)
    : user_dao_(std::move(user_dao)), room_dao_(std::move(room_dao)), 
      gen_(rd_()), dis_(1000, 9999) {
}

ServiceResult<ChatService::LoginData> ChatService::login(const std::string& email, const std::string& password, 
                                                        std::shared_ptr<Connection> conn) {
    // 验证输入
    if (email.empty() || password.empty()) {
        return ServiceResult<LoginData>::Fail(ErrorCode::BAD_REQUEST, "Email and password cannot be empty");
    }
    
    if (!isValidEmail(email)) {
        return ServiceResult<LoginData>::Fail(ErrorCode::BAD_REQUEST, "Invalid email format");
    }
    
    // 验证用户凭据
    if (!user_dao_->authenticateUser(email, password)) {
        return ServiceResult<LoginData>::Fail(ErrorCode::UNAUTHORIZED, "Invalid email or password");
    }
    
    // 获取用户信息
    auto user = user_dao_->getUserByEmail(email);
    if (!user) {
        return ServiceResult<LoginData>::Fail(ErrorCode::INTERNAL_ERROR, "User not found");
    }
    
    // 检查用户是否已在线
    if (user->is_online) {
        return ServiceResult<LoginData>::Fail(ErrorCode::CONFLICT, "User is already logged in");
    }
    
    // 更新用户在线状态
    if (!user_dao_->setUserOnline(user->id, true)) {
        return ServiceResult<LoginData>::Fail(ErrorCode::INTERNAL_ERROR, "Failed to update user status");
    }
    
    // 创建用户会话
    std::string session_id = generateSessionId();
    std::string display_name = user->getFullName();
    
    UserSession session(user->id, session_id, conn, display_name, user->role);
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        user_sessions_[user->id] = session;
    }
    
    LoginData data;
    data.user_id = user->id;
    data.display_name = display_name;
    data.role = user->role;
    
    return ServiceResult<LoginData>::Ok(data, "Login successful");
}

ServiceResult<ChatService::RegisterData> ChatService::registerUser(const std::string& email, const std::string& password, 
                                                                  const std::string& name) {
    // 验证输入
    if (email.empty() || password.empty() || name.empty()) {
        return ServiceResult<RegisterData>::Fail(ErrorCode::BAD_REQUEST, "All fields are required");
    }
    
    if (!isValidEmail(email)) {
        return ServiceResult<RegisterData>::Fail(ErrorCode::BAD_REQUEST, "Invalid email format");
    }
    
    if (!isValidDisplayName(name)) {
        return ServiceResult<RegisterData>::Fail(ErrorCode::BAD_REQUEST, 
            "Display name must be " + std::to_string(MIN_DISPLAY_NAME_LENGTH) + "-" + 
            std::to_string(MAX_DISPLAY_NAME_LENGTH) + " characters");
    }
    
    // 检查邮箱是否已存在
    if (user_dao_->isEmailExists(email)) {
        return ServiceResult<RegisterData>::Fail(ErrorCode::CONFLICT, "Email already exists");
    }
    
    // 生成唯一discriminator
    std::string discriminator = user_dao_->generateDiscriminator(name);
    
    // 创建用户
    User user(name, email, PasswordHasher::hashPassword(password, "salt"));
    user.discriminator = discriminator;
    user.role = UserRole::NORMAL;
    
    if (user_dao_->createUser(user)) {
        RegisterData data;
        data.user_id = user.id;
        data.display_name = user.getFullName();
        return ServiceResult<RegisterData>::Ok(data, "Registration successful");
    } else {
        return ServiceResult<RegisterData>::Fail(ErrorCode::INTERNAL_ERROR, "Registration failed");
    }
}

ServiceResult<void> ChatService::logout(int userId) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = user_sessions_.find(userId);
    if (it == user_sessions_.end()) {
        return ServiceResult<void>::Fail(ErrorCode::UNAUTHORIZED, "User not logged in");
    }
    
    // 更新用户在线状态
    user_dao_->setUserOnline(userId, false);
    
    // 移除会话
    user_sessions_.erase(it);
    
    return ServiceResult<void>::Ok("Logout successful");
}

ServiceResult<std::vector<Room>> ChatService::getRoomList() {
    try {
        auto rooms = room_dao_->getActiveRooms();
        return ServiceResult<std::vector<Room>>::Ok(rooms, "Room list retrieved successfully");
    } catch (const std::exception& e) {
        return ServiceResult<std::vector<Room>>::Fail(ErrorCode::INTERNAL_ERROR, "Failed to get room list");
    }
}

ServiceResult<void> ChatService::joinRoom(int userId, int roomId) {
    // 检查用户是否在线
    if (!isUserOnline(userId)) {
        return ServiceResult<void>::Fail(ErrorCode::UNAUTHORIZED, "User not logged in");
    }
    
    // 检查房间是否存在
    auto room = room_dao_->getRoomById(roomId);
    if (!room) {
        return ServiceResult<void>::Fail(ErrorCode::NOT_FOUND, "Room not found");
    }
    
    // 检查用户是否已在房间中
    if (isUserInRoom(userId, roomId)) {
        return ServiceResult<void>::Fail(ErrorCode::CONFLICT, "User already in room");
    }
    
    // 获取用户信息
    auto user = user_dao_->getUserById(userId);
    if (!user) {
        return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "User not found");
    }
    
    // 添加用户到房间
    if (room_dao_->addUserToRoom(roomId, userId)) {
        addUserToRoomInternal(userId, roomId, user->getFullName(), user->role);
        
        // 通知其他用户
        notifyUserJoinRoom(userId, roomId);
        
        return ServiceResult<void>::Ok("Joined room successfully");
    } else {
        return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "Failed to join room");
    }
}

ServiceResult<void> ChatService::leaveRoom(int userId, int roomId) {
    // 检查用户是否在线
    if (!isUserOnline(userId)) {
        return ServiceResult<void>::Fail(ErrorCode::UNAUTHORIZED, "User not logged in");
    }
    
    // 检查用户是否在房间中
    if (!isUserInRoom(userId, roomId)) {
        return ServiceResult<void>::Fail(ErrorCode::NOT_FOUND, "User not in room");
    }
    
    // 从房间移除用户
    if (room_dao_->removeUserFromRoom(roomId, userId)) {
        removeUserFromRoomInternal(userId, roomId);
        
        // 通知其他用户
        notifyUserLeaveRoom(userId, roomId);
        
        return ServiceResult<void>::Ok("Left room successfully");
    } else {
        return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "Failed to leave room");
    }
}

ServiceResult<ChatService::SendMessageData> ChatService::sendMessage(int userId, int roomId, const std::string& content) {
    // 验证输入
    if (content.empty()) {
        return ServiceResult<SendMessageData>::Fail(ErrorCode::BAD_REQUEST, "Message content cannot be empty");
    }
    
    if (content.length() > MAX_MESSAGE_LENGTH) {
        return ServiceResult<SendMessageData>::Fail(ErrorCode::BAD_REQUEST, 
            "Message too long (max " + std::to_string(MAX_MESSAGE_LENGTH) + " characters)");
    }
    
    // 检查用户是否在线
    if (!isUserOnline(userId)) {
        return ServiceResult<SendMessageData>::Fail(ErrorCode::UNAUTHORIZED, "User not logged in");
    }
    
    // 检查用户是否在房间中
    if (!isUserInRoom(userId, roomId)) {
        return ServiceResult<SendMessageData>::Fail(ErrorCode::NOT_FOUND, "User not in room");
    }
    
    // 获取用户信息
    auto user = user_dao_->getUserById(userId);
    if (!user) {
        return ServiceResult<SendMessageData>::Fail(ErrorCode::INTERNAL_ERROR, "User not found");
    }
    
    // 创建消息
    ChatMessage message;
    message.user_id = userId;
    message.room_id = roomId;
    message.content = content;
    message.display_name = user->getFullName();
    message.timestamp = getCurrentTimestamp();
    
    // 这里应该保存消息到数据库，暂时使用模拟的message_id
    message.message_id = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    // 通知房间内其他用户
    notifyNewMessage(roomId, content, userId);
    
    SendMessageData data;
    data.message_id = message.message_id;
    
    return ServiceResult<SendMessageData>::Ok(data, "Message sent successfully");
}

ServiceResult<ChatService::MessageHistoryData> ChatService::getMessageHistory(int roomId, int limit, int offset) {
    // 检查房间是否存在
    auto room = room_dao_->getRoomById(roomId);
    if (!room) {
        return ServiceResult<MessageHistoryData>::Fail(ErrorCode::NOT_FOUND, "Room not found");
    }
    
    // 这里应该从数据库获取消息历史，暂时返回空数据
    MessageHistoryData data;
    data.messages = std::vector<ChatMessage>();
    data.total = 0;
    
    return ServiceResult<MessageHistoryData>::Ok(data, "Message history retrieved successfully");
}

ServiceResult<std::vector<User>> ChatService::getOnlineUsers(int roomId) {
    // 检查房间是否存在
    auto room = room_dao_->getRoomById(roomId);
    if (!room) {
        return ServiceResult<std::vector<User>>::Fail(ErrorCode::NOT_FOUND, "Room not found");
    }
    
    // 获取房间成员
    auto roomMembers = room_dao_->getRoomMemberDetails(roomId);
    std::vector<User> onlineUsers;
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (const auto& member : roomMembers) {
        if (user_sessions_.find(member.user_id) != user_sessions_.end()) {
            User user;
            user.id = member.user_id;
            user.name = member.name;
            user.discriminator = member.discriminator;
            user.email = member.email;
            user.role = member.role;
            user.is_online = true;
            onlineUsers.push_back(user);
        }
    }
    
    return ServiceResult<std::vector<User>>::Ok(onlineUsers, "Online users retrieved successfully");
}

void ChatService::notifyUserJoinRoom(int userId, int roomId) {
    auto user = user_dao_->getUserById(userId);
    if (!user) return;
    
    Json::Value message;
    message["type"] = MSG_USER_JOIN_PUSH;
    message["room_id"] = roomId;
    message["user_id"] = userId;
    message["display_name"] = user->getFullName();
    
    Json::FastWriter writer;
    std::string jsonMessage = writer.write(message);
    
    broadcastToRoom(roomId, jsonMessage, userId);
}

void ChatService::notifyUserLeaveRoom(int userId, int roomId) {
    auto user = user_dao_->getUserById(userId);
    if (!user) return;
    
    Json::Value message;
    message["type"] = MSG_USER_LEAVE_PUSH;
    message["room_id"] = roomId;
    message["user_id"] = userId;
    message["display_name"] = user->getFullName();
    
    Json::FastWriter writer;
    std::string jsonMessage = writer.write(message);
    
    broadcastToRoom(roomId, jsonMessage, userId);
}

void ChatService::notifyNewMessage(int roomId, const std::string& message, int senderId) {
    auto user = user_dao_->getUserById(senderId);
    if (!user) return;
    
    Json::Value jsonMessage;
    jsonMessage["type"] = MSG_CHAT_MESSAGE_PUSH;
    jsonMessage["room_id"] = roomId;
    jsonMessage["user_id"] = senderId;
    jsonMessage["display_name"] = user->getFullName();
    jsonMessage["content"] = message;
    jsonMessage["timestamp"] = getCurrentTimestamp();
    
    Json::FastWriter writer;
    std::string messageStr = writer.write(jsonMessage);
    
    broadcastToRoom(roomId, messageStr, senderId);
}

void ChatService::notifyUserOnline(int userId) {
    // 实现用户上线通知
}

void ChatService::notifyUserOffline(int userId) {
    // 实现用户下线通知
}

void ChatService::handleUserDisconnect(int fd) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // 查找对应的用户会话
    for (auto it = user_sessions_.begin(); it != user_sessions_.end(); ++it) {
        auto conn = it->second.getConnection();
        if (conn && conn->getFd() == fd) {
            // 更新用户在线状态
            user_dao_->setUserOnline(it->first, false);
            
            // 移除会话
            user_sessions_.erase(it);
            break;
        }
    }
}

void ChatService::cleanupTimeoutSessions(std::chrono::seconds timeout) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = user_sessions_.begin();
    while (it != user_sessions_.end()) {
        if (it->second.isTimeout(timeout)) {
            // 更新用户在线状态
            user_dao_->setUserOnline(it->first, false);
            
            // 移除会话
            it = user_sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void ChatService::updateUserActivity(int userId) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto it = user_sessions_.find(userId);
    if (it != user_sessions_.end()) {
        it->second.updateActivity();
    }
}

// 私有方法实现

std::string ChatService::generateSessionId() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d%H%M%S");
    oss << std::setfill('0') << std::setw(3) << ms.count();
    oss << "_" << dis_(gen_);
    
    return oss.str();
}

bool ChatService::isValidDisplayName(const std::string& display_name) {
    return display_name.length() >= MIN_DISPLAY_NAME_LENGTH && 
           display_name.length() <= MAX_DISPLAY_NAME_LENGTH;
}

bool ChatService::isValidEmail(const std::string& email) {
    return email.find('@') != std::string::npos && email.length() >= 5;
}

std::string ChatService::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string ChatService::errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "Success";
        case ErrorCode::BAD_REQUEST: return "Bad Request";
        case ErrorCode::UNAUTHORIZED: return "Unauthorized";
        case ErrorCode::NOT_FOUND: return "Not Found";
        case ErrorCode::CONFLICT: return "Conflict";
        case ErrorCode::INTERNAL_ERROR: return "Internal Server Error";
        default: return "Unknown Error";
    }
}

bool ChatService::isUserOnline(int userId) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return user_sessions_.find(userId) != user_sessions_.end();
}

bool ChatService::isUserInRoom(int userId, int roomId) {
    std::lock_guard<std::mutex> lock(room_users_mutex_);
    auto it = room_users_.find(roomId);
    if (it != room_users_.end()) {
        return it->second.find(userId) != it->second.end();
    }
    return false;
}

void ChatService::addUserToRoomInternal(int userId, int roomId, const std::string& displayName, UserRole role) {
    std::lock_guard<std::mutex> lock(room_users_mutex_);
    room_users_[roomId].insert(userId);
    
    // 更新用户当前房间
    std::lock_guard<std::mutex> sessionLock(sessions_mutex_);
    auto it = user_sessions_.find(userId);
    if (it != user_sessions_.end()) {
        it->second.current_room_id = roomId;
    }
}

void ChatService::removeUserFromRoomInternal(int userId, int roomId) {
    std::lock_guard<std::mutex> lock(room_users_mutex_);
    auto it = room_users_.find(roomId);
    if (it != room_users_.end()) {
        it->second.erase(userId);
    }
    
    // 更新用户当前房间
    std::lock_guard<std::mutex> sessionLock(sessions_mutex_);
    auto sessionIt = user_sessions_.find(userId);
    if (sessionIt != user_sessions_.end()) {
        sessionIt->second.current_room_id = -1;
    }
}

void ChatService::broadcastToRoom(int roomId, const std::string& message, int excludeUserId) {
    std::lock_guard<std::mutex> lock(room_users_mutex_);
    auto it = room_users_.find(roomId);
    if (it == room_users_.end()) return;
    
    std::lock_guard<std::mutex> sessionLock(sessions_mutex_);
    for (int userId : it->second) {
        if (userId == excludeUserId) continue;
        
        auto sessionIt = user_sessions_.find(userId);
        if (sessionIt != user_sessions_.end()) {
            auto conn = sessionIt->second.getConnection();
            if (conn && !conn->isClosed()) {
                // 这里需要根据消息类型发送，暂时使用系统消息类型
                conn->sendMessage(MSG_SYSTEM_MESSAGE_PUSH, message);
            }
        }
    }
}