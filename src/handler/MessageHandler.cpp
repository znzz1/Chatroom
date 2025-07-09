#include "handler/MessageHandler.h"
#include "service/ChatService.h"
#include "dao/UserDao.h"
#include "dao/RoomDao.h"
#include "utils/PasswordHasher.h"
#include "protocol/MessageType.h"
#include <jsoncpp/json/json.h>
#include <iostream>
#include <sstream>
#include <vector>

MessageHandler::MessageHandler(std::shared_ptr<ChatService> chatService)
    : chatService_(chatService) {}

void MessageHandler::handleMessage(const Message& message, std::shared_ptr<Connection> conn) {
    try {
        std::cout << "[MessageHandler] Processing message type: " << message.type << std::endl;
        
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(message.data, root)) {
            std::cerr << "[MessageHandler] Failed to parse JSON: " << message.data << std::endl;
            sendErrorResponse(conn, "Invalid JSON format", 400);
            return;
        }
        
        switch (message.type) {
            case MSG_LOGIN:      handleLogin(root, conn); break;
            case MSG_REGISTER:   handleRegister(root, conn); break;
            case MSG_LOGOUT:     handleLogout(root, conn); break;
            case MSG_GET_ROOM_LIST: handleGetRoomList(root, conn); break;
            case MSG_JOIN_ROOM:  handleJoinRoom(root, conn); break;
            case MSG_LEAVE_ROOM: handleLeaveRoom(root, conn); break;
            case MSG_SEND_MESSAGE: handleSendMessage(root, conn); break;
            case MSG_GET_MESSAGE_HISTORY: handleGetMessageHistory(root, conn); break;
            case MSG_GET_ONLINE_USERS: handleGetOnlineUsers(root, conn); break;
            default:
                std::cerr << "[MessageHandler] Unknown message type: " << message.type << std::endl;
                sendErrorResponse(conn, "Unknown message type", 400);
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling message: " << e.what() << std::endl;
        sendErrorResponse(conn, "Internal server error", 500);
    }
}

void MessageHandler::handleLogin(const Json::Value& root, std::shared_ptr<Connection> conn) {
    if (!check(root, conn, {"email", "password"})) {
        return;
    }
    
    std::string email = root["email"].asString();
    std::string password = root["password"].asString();
    
    auto result = chatService_->login(email, password, conn);
    
    Json::Value response = buildResponse("login_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        response["data"]["user_id"] = result.data.user_id;
        response["data"]["display_name"] = result.data.display_name;
        response["data"]["role"] = static_cast<int>(result.data.role);
    }
    
    sendResponse(conn, MSG_LOGIN_RESPONSE, response);
}

void MessageHandler::handleRegister(const Json::Value& root, std::shared_ptr<Connection> conn) {
    if (!check(root, conn, {"email", "password", "name"})) {
        return;
    }
    
    std::string email = root["email"].asString();
    std::string password = root["password"].asString();
    std::string name = root["name"].asString();
    
    auto result = chatService_->registerUser(email, password, name);
    
    Json::Value response = buildResponse("register_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        response["data"]["user_id"] = result.data.user_id;
        response["data"]["display_name"] = result.data.display_name;
    }
    
    sendResponse(conn, MSG_REGISTER_RESPONSE, response);
}

void MessageHandler::handleLogout(const Json::Value& root, std::shared_ptr<Connection> conn) {
    if (!check(root, conn, {"user_id"})) return;
    auto result = chatService_->logout(root["user_id"].asInt());
    sendResponse(conn, MSG_LOGOUT_RESPONSE, buildResponse("logout_response", result.ok, static_cast<int>(result.code), result.message));
}

void MessageHandler::handleGetRoomList(const Json::Value& root, std::shared_ptr<Connection> conn) {
    auto result = chatService_->getRoomList();
    
    Json::Value response = buildResponse("room_list_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        Json::Value roomsArray;
        for (const auto& room : result.data) {
            Json::Value roomObj;
            roomObj["room_id"] = room.id;
            roomObj["name"] = room.name;
            roomObj["description"] = room.description;
            roomObj["member_count"] = room.current_users;
            roomObj["is_public"] = true;
            roomsArray.append(roomObj);
        }
        response["data"]["rooms"] = roomsArray;
    }
    
    sendResponse(conn, MSG_ROOM_LIST_RESPONSE, response);
}

void MessageHandler::handleJoinRoom(const Json::Value& root, std::shared_ptr<Connection> conn) {
    if (!check(root, conn, {"user_id", "room_id"})) return;
    auto result = chatService_->joinRoom(root["user_id"].asInt(), root["room_id"].asInt());
    sendResponse(conn, MSG_JOIN_ROOM_RESPONSE, buildResponse("join_room_response", result.ok, static_cast<int>(result.code), result.message));
}

void MessageHandler::handleLeaveRoom(const Json::Value& root, std::shared_ptr<Connection> conn) {
    if (!check(root, conn, {"user_id", "room_id"})) return;
    auto result = chatService_->leaveRoom(root["user_id"].asInt(), root["room_id"].asInt());
    sendResponse(conn, MSG_LEAVE_ROOM_RESPONSE, buildResponse("leave_room_response", result.ok, static_cast<int>(result.code), result.message));
}

void MessageHandler::handleSendMessage(const Json::Value& root, std::shared_ptr<Connection> conn) {
    if (!check(root, conn, {"user_id", "room_id", "content"})) {
        return;
    }
    
    int userId = root["user_id"].asInt();
    int roomId = root["room_id"].asInt();
    std::string content = root["content"].asString();
    
    auto result = chatService_->sendMessage(userId, roomId, content);
    
    Json::Value response = buildResponse("send_message_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        response["data"]["message_id"] = result.data.message_id;
    }
    
    sendResponse(conn, MSG_SEND_MESSAGE_RESPONSE, response);
}

void MessageHandler::handleGetMessageHistory(const Json::Value& root, std::shared_ptr<Connection> conn) {
    if (!check(root, conn, {"room_id"})) {
        return;
    }
    
    int roomId = root["room_id"].asInt();
    int limit = root.get("limit", 50).asInt();
    int offset = root.get("offset", 0).asInt();
    
    auto result = chatService_->getMessageHistory(roomId, limit, offset);
    
    Json::Value response = buildResponse("message_history_response", result.ok, static_cast<int>(result.code), result.message);
    
    if (result.ok) {
        Json::Value messagesArray;
        for (const auto& msg : result.data.messages) {
            Json::Value msgObj;
            msgObj["message_id"] = msg.message_id;
            msgObj["user_id"] = msg.user_id;
            msgObj["display_name"] = msg.display_name;
            msgObj["room_id"] = msg.room_id;
            msgObj["content"] = msg.content;
            msgObj["timestamp"] = msg.timestamp;
            messagesArray.append(msgObj);
        }
        response["data"]["messages"] = messagesArray;
        response["data"]["total"] = result.data.total;
    }
    
    sendResponse(conn, MSG_MESSAGE_HISTORY_RESPONSE, response);
}

void MessageHandler::handleGetOnlineUsers(const Json::Value& root, std::shared_ptr<Connection> conn) {
    if (!check(root, conn, {"room_id"})) return;
    auto result = chatService_->getOnlineUsers(root["room_id"].asInt());
    Json::Value response = buildResponse("online_users_response", result.ok, static_cast<int>(result.code), result.message);
    if (result.ok) {
        Json::Value usersArray;
        for (const auto& user : result.data) {
            Json::Value userObj;
            userObj["user_id"] = user.id;
            userObj["display_name"] = user.getFullName();
            userObj["role"] = static_cast<int>(user.role);
            userObj["is_online"] = user.is_online;
            usersArray.append(userObj);
        }
        response["data"]["users"] = usersArray;
    }
    sendResponse(conn, MSG_ONLINE_USERS_RESPONSE, response);
}

void MessageHandler::sendResponse(std::shared_ptr<Connection> conn, uint16_t type, const Json::Value& data) {
    if (!conn || conn->isClosed()) return;
    
    try {
        Json::FastWriter writer;
        conn->sendMessage(type, writer.write(data));
    } catch (const std::exception& e) {
        std::cerr << "[MessageHandler] Error sending response: " << e.what() << std::endl;
        sendErrorResponse(conn, "Failed to send response", 500);
    }
}

void MessageHandler::sendErrorResponse(std::shared_ptr<Connection> conn, const std::string& message, int code) {
    if (!conn || conn->isClosed()) return;
    
    try {
        Json::Value response = buildResponse("error_response", false, code, message);
        Json::FastWriter writer;
        conn->sendMessage(MSG_ERROR_RESPONSE, writer.write(response));
    } catch (const std::exception& e) {
        std::cerr << "[MessageHandler] Error sending error response: " << e.what() << std::endl;
    }
}

bool MessageHandler::check(const Json::Value& root, std::shared_ptr<Connection> conn, const std::vector<std::string>& requiredFields) {
    for (const auto& field : requiredFields) {
        if (!root.isMember(field)) {
            sendErrorResponse(conn, "Missing required field: " + field, 400);
            return false;
        }
        if (root[field].isString() && root[field].asString().empty()) {
            sendErrorResponse(conn, "Field cannot be empty: " + field, 400);
            return false;
        }
        if (root[field].isInt() && root[field].asInt() <= 0) {
            sendErrorResponse(conn, "Invalid value for field: " + field, 400);
            return false;
        }
    }
    return true;
}

Json::Value MessageHandler::buildResponse(const std::string& type, bool success, int code, const std::string& message) const {
    Json::Value response;
    response["type"] = type;
    response["success"] = success;
    response["code"] = code;
    if (!message.empty()) response["message"] = message;
    return response;
} 