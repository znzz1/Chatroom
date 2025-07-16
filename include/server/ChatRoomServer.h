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

struct RoomInfo {
    std::string name;
    std::string description;
    int max_users;
    int creator_id;
    std::string created_time;
    std::vector<int> users;
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

    unordered_map<int, RoomInfo> rooms_;
    std::mutex rooms_mutex_;

    unordered_map<int, int> userId_to_fd_;
    std::mutex userId_to_fd_mutex_;

    unordered_map<int, int> userId_to_roomId_;
    std::mutex userId_to_roomId_mutex_;
    
    std::atomic<bool> running_{false};
    
    void setupServer();
    void setupServices();
    void loadRoomsFromDatabase();
    














    void handleEvent(const epoll_event& ev);
    void handleNewConnection();
    void handleReadEvent(int fd);
    void handleWriteEvent(int fd);
    void handleConnectionError(int fd);
    void handleConnectionClose(int fd);
    
    void processReadTask(int fd);
    void processWriteTask(int fd);
    void processMessage(int fd, const Message& message);
    
    void handleApiRequest(int fd, const Message& message);
    void handleLogin(int fd, const Json::Value& root);
    void handleRegister(int fd, const Json::Value& root);
    void handleGetActiveRooms(int fd);
    void handleGetRoomInfo(int fd, const Json::Value& root);
    void handleCreateRoom(int fd, const Json::Value& root);
    void handleGetMessageHistory(int fd, const Json::Value& root);
    
    void handleRealTimeMessage(int fd, const Message& message);
    void handleSendMessage(int fd, const Message& message);
    
    void broadcastMessageToRoom(int roomId, int senderId, const std::string& content, const std::string& displayName);
    void notifyUserJoinRoom(int roomId, int userId, const std::string& displayName);
    void notifyUserLeaveRoom(int roomId, int userId, const std::string& displayName);
    
    void trySendData(int fd);
    void sendResponse(int fd, uint16_t type, const Json::Value& data);
    void sendErrorResponse(int fd, const std::string& message, int code);
    Json::Value buildResponse(const std::string& type, bool success, int code, const std::string& message = "") const;
    
    void addOnlineUser(int userId, const std::string& displayName, int roomId, std::shared_ptr<Connection> conn);
    void removeOnlineUser(int userId);
    void moveUserToRoom(int userId, int newRoomId);
}; 