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

constexpr int ROOM_ID_NONE = -1;
constexpr int EPOLL_TIMEOUT_MS = 1000;
constexpr int MAX_CONNECTIONS = 10000;
constexpr int MAX_ACCEPT_PER_LOOP = 100;
constexpr int MAX_READ_BUFFER_SIZE = 1024 * 1024;
constexpr int MAX_WRITE_BUFFER_SIZE = 1024 * 1024;

struct RoomInfo {
    std::string name;
    std::string description;
    int max_users;
    int creator_id;
    std::string created_time;
    std::unordered_set<int> users;
    std::mutex users_mutex;

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
    ThreadPool thread_pool_;
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
    std::mutex connections_mutex_;
    
    std::shared_ptr<ServiceManager> service_manager_;
    
    unordered_map<int, RoomInfo> active_rooms_;
    std::mutex active_rooms_mutex_;

    unordered_map<int, RoomInfo> inactive_rooms_;
    std::mutex inactive_rooms_mutex_;

    unordered_map<int, int> fd_to_userId_;
    std::mutex fd_to_userId_mutex_;

    unordered_map<int, int> userId_to_fd_;
    std::mutex userId_to_fd_mutex_;

    unordered_map<int, int> userId_to_roomId_;
    std::mutex userId_to_roomId_mutex_;
    
    std::atomic<bool> running_{false};
    
    void setupServer();
    void setupServices();
    void loadRoomsFromDatabase();
    
private:
    void handleEvent(const epoll_event& ev);
    void handleNewConnection();
    void handleReadEvent(int fd);
    void handleWriteEvent(int fd);
    void handleConnectionError(int fd);
    void handleConnectionClose(int fd);

private:
    void handleRequest(int fd, const Message& message);
    void handleRegister(int fd, const Json::Value& root);
    void handleChangePassword(int fd, const Json::Value& root);
    void handleChangeDisplayName(int fd, const Json::Value& root);
    void handleLogin(int fd, const Json::Value& root);
    void handleLogout(int fd, const Json::Value& root);
    void handleGetActiveRooms(int fd);
    void handleGetAllRooms(int fd);
    void handleGetRoomInfo(int fd, const Json::Value& root);
    void handleCreateRoom(int fd, const Json::Value& root);
    void handleDeleteRoom(int fd, const Json::Value& root);
    void handleSetRoomName(int fd, const Json::Value& root);
    void handleSetRoomDescription(int fd, const Json::Value& root);
    void handleSetRoomMaxUsers(int fd, const Json::Value& root);
    void handleSetRoomStatus(int fd, const Json::Value& root);
    void handleSendMessage(int fd, const Json::Value& root);
    void handleGetMessageHistory(int fd, const Json::Value& root);
    void handleJoinRoom(int fd, const Json::Value& root);
    void handleLeaveRoom(int fd, const Json::Value& root);
    void handleGetRoomMembers(int fd, const Json::Value& root);
    void handleGetUserInfo(int fd, const Json::Value& root);

private:
    void notifyUserJoinRoom(int roomId, int userId, const std::string& displayName);
    void notifyUserLeaveRoom(int roomId, int userId);

    void notifyRoomNameUpdate(int roomId, const std::string& newName);
    void notifyRoomDescriptionUpdate(int roomId, const std::string& newDescription);
    void notifyRoomMaxUsersUpdate(int roomId, int newMaxUsers);
    void notifyRoomStatusChange(int roomId, int newStatus);

    void broadcastMessageToRoom(int roomId, int senderId, const std::string& content, const std::string& displayName);
}; 