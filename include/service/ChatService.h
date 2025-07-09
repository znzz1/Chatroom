#pragma once
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <random>
#include <string>
#include <vector>
#include <chrono>
#include "net/Connection.h"
#include "dao/RoomDao.h"
#include "dao/UserDao.h"

// 统一错误码
enum class ErrorCode {
    SUCCESS = 200,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    NOT_FOUND = 404,
    CONFLICT = 409,
    INTERNAL_ERROR = 500
};

// 统一结果结构
// 主模板
template<typename T>
struct ServiceResult {
    bool ok;
    ErrorCode code;
    std::string message;
    T data;

    ServiceResult() : ok(false), code(ErrorCode::INTERNAL_ERROR) {}
    ServiceResult(bool o, ErrorCode c, const std::string& msg) : ok(o), code(c), message(msg) {}
    ServiceResult(bool o, ErrorCode c, const std::string& msg, const T& d) : ok(o), code(c), message(msg), data(d) {}

    static ServiceResult<T> Ok(const T& d, const std::string& msg = "Success") { return ServiceResult<T>(true, ErrorCode::SUCCESS, msg, d); }
    static ServiceResult<T> Fail(ErrorCode c, const std::string& msg) { return ServiceResult<T>(false, c, msg); }
};
// void 特化
template<>
struct ServiceResult<void> {
    bool ok;
    ErrorCode code;
    std::string message;

    ServiceResult() : ok(false), code(ErrorCode::INTERNAL_ERROR) {}
    ServiceResult(bool o, ErrorCode c, const std::string& msg) : ok(o), code(c), message(msg) {}

    static ServiceResult<void> Ok(const std::string& msg = "Success") { return ServiceResult<void>(true, ErrorCode::SUCCESS, msg); }
    static ServiceResult<void> Fail(ErrorCode c, const std::string& msg) { return ServiceResult<void>(false, c, msg); }
};

// 用户会话信息
struct UserSession {
    int user_id;
    std::string session_id;
    std::weak_ptr<Connection> connection;
    std::string display_name;
    UserRole role;
    std::chrono::steady_clock::time_point last_activity;
    int current_room_id;
    
    UserSession() : user_id(0), role(UserRole::NORMAL), current_room_id(-1) {}
    UserSession(int uid, const std::string& sid, std::shared_ptr<Connection> conn, 
                const std::string& name, UserRole r)
        : user_id(uid), session_id(sid), connection(conn), display_name(name), 
          role(r), last_activity(std::chrono::steady_clock::now()), current_room_id(-1) {}
    
    // 检查连接是否有效
    bool isConnected() const {
        return !connection.expired();
    }
    
    // 获取连接指针
    std::shared_ptr<Connection> getConnection() const {
        return connection.lock();
    }
    
    // 更新活动时间
    void updateActivity() {
        last_activity = std::chrono::steady_clock::now();
    }
    
    // 检查是否超时
    bool isTimeout(std::chrono::seconds timeout) const {
        return std::chrono::steady_clock::now() - last_activity > timeout;
    }
};

// 聊天消息
struct ChatMessage {
    int message_id;
    int user_id;
    int room_id;
    std::string content;
    std::string display_name;
    std::string timestamp;
    
    ChatMessage() : message_id(0), user_id(0), room_id(0) {}
};



class ChatService {
public:
    ChatService(std::unique_ptr<UserDao> user_dao, std::unique_ptr<RoomDao> room_dao);
    ~ChatService() = default;
    
    // 禁用拷贝
    ChatService(const ChatService&) = delete;
    ChatService& operator=(const ChatService&) = delete;
    
    // 用户管理
    struct LoginData {
        int user_id;
        std::string display_name;
        UserRole role;
    };
    
    struct RegisterData {
        int user_id;
        std::string display_name;
    };
    
    ServiceResult<LoginData> login(const std::string& email, const std::string& password, 
                                  std::shared_ptr<Connection> conn);
    ServiceResult<RegisterData> registerUser(const std::string& email, const std::string& password, 
                                            const std::string& name);
    ServiceResult<void> logout(int userId);
    
    // 房间管理
    ServiceResult<std::vector<Room>> getRoomList();
    ServiceResult<void> joinRoom(int userId, int roomId);
    ServiceResult<void> leaveRoom(int userId, int roomId);
    
    // 消息管理
    struct SendMessageData {
        int message_id;
    };
    
    struct MessageHistoryData {
        std::vector<ChatMessage> messages;
        int total;
    };
    
    ServiceResult<SendMessageData> sendMessage(int userId, int roomId, const std::string& content);
    ServiceResult<MessageHistoryData> getMessageHistory(int roomId, int limit = 50, int offset = 0);
    
    // 用户查询
    ServiceResult<std::vector<User>> getOnlineUsers(int roomId);
    
    // 通知相关方法
    void notifyUserJoinRoom(int userId, int roomId);
    void notifyUserLeaveRoom(int userId, int roomId);
    void notifyNewMessage(int roomId, const std::string& message, int senderId);
    void notifyUserOnline(int userId);
    void notifyUserOffline(int userId);
    
    // 连接管理
    void handleUserDisconnect(int fd);
    
    // 维护方法
    void cleanupTimeoutSessions(std::chrono::seconds timeout = std::chrono::seconds(300));
    void updateUserActivity(int userId);
    
private:
    // 工具方法
    std::string generateSessionId();
    bool isValidDisplayName(const std::string& display_name);
    bool isValidEmail(const std::string& email);
    std::string getCurrentTimestamp();
    std::string errorCodeToString(ErrorCode code);
    
    // 内部管理方法
    bool isUserOnline(int userId);
    bool isUserInRoom(int userId, int roomId);
    void addUserToRoomInternal(int userId, int roomId, const std::string& displayName, UserRole role);
    void removeUserFromRoomInternal(int userId, int roomId);
    void broadcastToRoom(int roomId, const std::string& message, int excludeUserId = -1);
    
    // 数据成员
    std::unique_ptr<UserDao> user_dao_;
    std::unique_ptr<RoomDao> room_dao_;
    
    // 用户会话管理
    std::unordered_map<int, UserSession> user_sessions_;
    mutable std::mutex sessions_mutex_;
    
    // 房间用户管理
    std::unordered_map<int, std::unordered_set<int>> room_users_;
    mutable std::mutex room_users_mutex_;
    
    // 随机数生成
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<> dis_;
    
    // 配置常量
    static constexpr std::chrono::seconds DEFAULT_SESSION_TIMEOUT{300};  // 5分钟
    static constexpr int MAX_MESSAGE_LENGTH = 1000;
    static constexpr int MAX_DISPLAY_NAME_LENGTH = 20;
    static constexpr int MIN_DISPLAY_NAME_LENGTH = 2;
}; 