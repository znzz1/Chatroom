#pragma once
#include <memory>
#include <unordered_map>
#include <mutex>
#include <unordered_set>
#include "net/EpollPoller.h"
#include "net/Connection.h"
#include "utils/ThreadPool.h"
#include "service/ServiceManager.h"
#include "server/Protocol.h"
#include <jsoncpp/json/json.h>
#include <thread>
#include <atomic>

constexpr int ROOM_ID_NONE = -1;
constexpr int EPOLL_TIMEOUT_MS = 1000;
constexpr int MAX_READ_BUFFER_SIZE = 1024 * 1024;
constexpr int MAX_WRITE_BUFFER_SIZE = 1024 * 1024;
constexpr int64_t TOKEN_EXPIRE_MINUTES = 30;

struct RoomInfo {
    std::string name;
    std::string description;
    int max_users;
    int creator_id;
    std::string created_time;
    std::unordered_set<int> users;

    RoomInfo() : name(), description(), max_users(0), creator_id(0), created_time(), users() {}
    RoomInfo(const std::string& name_, const std::string& description_, int max_users_, int creator_id_, const std::string& created_time_)
        : name(name_),
          description(description_),
          max_users(max_users_),
          creator_id(creator_id_),
          created_time(created_time_) {}
};

class ChatRoomServer {
public:
    explicit ChatRoomServer();
    ~ChatRoomServer();
    void run();    
    void stop();
    
private:
    int listen_fd_;
    uint16_t port_;
    EpollPoller poller_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
    std::mutex connections_mutex_;
    
    std::shared_ptr<ServiceManager> service_manager_;
    
    std::unordered_map<int, RoomInfo> active_rooms_;
    std::mutex active_rooms_mutex_;

    std::unordered_map<int, RoomInfo> inactive_rooms_;
    std::mutex inactive_rooms_mutex_;

    std::unordered_map<int, int> fd_to_userId_;
    std::mutex fd_to_userId_mutex_;

    std::unordered_map<int, int> userId_to_fd_;
    std::mutex userId_to_fd_mutex_;

    std::unordered_map<int, int> userId_to_roomId_;
    std::mutex userId_to_roomId_mutex_;
    
    std::unordered_map<int, std::pair<std::string, int64_t>> userId_to_token_; 
    std::mutex userId_to_token_mutex_;
    
    std::atomic<int> token_counter_{0};
    
    std::atomic<bool> running_{false};
    
    std::thread cleanup_thread_;
    std::atomic<bool> cleanup_running_{true};
    
    void setupServer();
    void setupServices();
    void loadRoomsFromDatabase();
    
private:
    void handleEvent(const epoll_event& ev);
    void handleNewConnection();
    void handleReadEvent(int fd);
    void handleWriteEvent(int fd);
    void handleConnectionError(int fd);
    void cleanupConnection(int fd);

private:
    void handleRequest(int fd, const NetworkMessage& message);
    void handleRegister(int fd, const std::string& data);
    void handleChangePassword(int fd, const std::string& data);
    void handleChangeDisplayName(int fd, const std::string& data);
    void handleLogin(int fd, const std::string& data);
    void handleLogout(int fd, const std::string& data);
    void handleFetchActiveRooms(int fd, const std::string& data);
    void handleFetchInactiveRooms(int fd, const std::string& data);
    void handleCreateRoom(int fd, const std::string& data);
    void handleDeleteRoom(int fd, const std::string& data);
    void handleSetRoomName(int fd, const std::string& data);
    void handleSetRoomDescription(int fd, const std::string& data);
    void handleSetRoomMaxUsers(int fd, const std::string& data);
    void handleSetRoomStatus(int fd, const std::string& data);
    void handleSendMessage(int fd, const std::string& data);
    void handleGetMessageHistory(int fd, const std::string& data);
    void handleJoinRoom(int fd, const std::string& data);
    void handleLeaveRoom(int fd, const std::string& data);
    void handleGetUserInfo(int fd, const std::string& data);

private:
    void notifyRoomUsers(int roomId, uint16_t messageType, const Json::Value& notification);

private:
    bool parseJson(const std::string& data, Json::Value& root);
    bool validateRequiredFields(const Json::Value& root, const std::vector<std::string>& requiredFields);
    void sendResponse(int fd, uint16_t responseType, const Json::Value& response);
    void sendErrorResponse(int fd, uint16_t responseType, const std::string& message);
    
private:
    std::string generateToken(int userId, const std::string& role);
    int validateToken(int fd, std::string& token);
    void cleanupExpiredTokens();
};