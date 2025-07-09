#pragma once
#include <memory>
#include <jsoncpp/json/json.h>
#include "net/Connection.h"
#include "protocol/MessageType.h"

class ChatService;

class MessageHandler {
public:
    explicit MessageHandler(std::shared_ptr<ChatService> chatService);
    ~MessageHandler() = default;
    
    void handleMessage(const Message& message, std::shared_ptr<Connection> conn);
    
private:
    void handleLogin(const Json::Value& root, std::shared_ptr<Connection> conn);
    void handleRegister(const Json::Value& root, std::shared_ptr<Connection> conn);
    void handleLogout(const Json::Value& root, std::shared_ptr<Connection> conn);
    void handleGetRoomList(const Json::Value& root, std::shared_ptr<Connection> conn);
    void handleJoinRoom(const Json::Value& root, std::shared_ptr<Connection> conn);
    void handleLeaveRoom(const Json::Value& root, std::shared_ptr<Connection> conn);
    void handleSendMessage(const Json::Value& root, std::shared_ptr<Connection> conn);
    void handleGetMessageHistory(const Json::Value& root, std::shared_ptr<Connection> conn);
    void handleGetOnlineUsers(const Json::Value& root, std::shared_ptr<Connection> conn);
    
    void sendResponse(std::shared_ptr<Connection> conn, uint16_t type, const Json::Value& data);
    void sendErrorResponse(std::shared_ptr<Connection> conn, const std::string& message, int code);
    bool check(const Json::Value& root, std::shared_ptr<Connection> conn, const std::vector<std::string>& requiredFields);
    Json::Value buildResponse(const std::string& type, bool success, int code, const std::string& message = "") const;
    
    std::shared_ptr<ChatService> chatService_;
}; 