#pragma once
#include <string>
#include <vector>
#include <json/json.h>

namespace protocol {

enum class MessageType {
    LOGIN_REQUEST = 1001,
    REGISTER_REQUEST = 1002,
    LOGOUT_REQUEST = 1003,
    GET_ROOM_LIST_REQUEST = 1004,
    JOIN_ROOM_REQUEST = 1005,
    LEAVE_ROOM_REQUEST = 1006,
    SEND_MESSAGE_REQUEST = 1007,
    GET_ROOM_MEMBERS_REQUEST = 1008,
    GET_USER_INFO_REQUEST = 1009,
    UPDATE_USER_INFO_REQUEST = 1010,
    
    CREATE_ROOM_REQUEST = 2001,
    DELETE_ROOM_REQUEST = 2002,
    UPDATE_ROOM_REQUEST = 2003,
    GET_SYSTEM_STATS_REQUEST = 2004,
    
    LOGIN_RESPONSE = 3001,
    REGISTER_RESPONSE = 3002,
    LOGOUT_RESPONSE = 3003,
    GET_ROOM_LIST_RESPONSE = 3004,
    JOIN_ROOM_RESPONSE = 3005,
    LEAVE_ROOM_RESPONSE = 3006,
    SEND_MESSAGE_RESPONSE = 3007,
    GET_ROOM_MEMBERS_RESPONSE = 3008,
    GET_USER_INFO_RESPONSE = 3009,
    UPDATE_USER_INFO_RESPONSE = 3010,
    
    CREATE_ROOM_RESPONSE = 4001,
    DELETE_ROOM_RESPONSE = 4002,
    UPDATE_ROOM_RESPONSE = 4003,
    GET_SYSTEM_STATS_RESPONSE = 4004,
    
    CHAT_MESSAGE_PUSH = 5001,
    SYSTEM_MESSAGE_PUSH = 5002,
    USER_JOIN_PUSH = 5003,
    USER_LEAVE_PUSH = 5004,
    ROOM_CREATED_PUSH = 5005,
    ROOM_DELETED_PUSH = 5006,
    ROOM_UPDATED_PUSH = 5007
};

enum class ResponseCode {
    SUCCESS = 200,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    INTERNAL_ERROR = 500,
    ROOM_FULL = 601,
    ROOM_NOT_FOUND = 602,
    USER_ALREADY_IN_ROOM = 603,
    USER_NOT_IN_ROOM = 604,
    INSUFFICIENT_PERMISSIONS = 605
};

struct Message {
    MessageType type;
    std::string session_id;
    Json::Value data;
    
    Message() : type(MessageType::LOGIN_REQUEST) {}
    Message(MessageType t) : type(t) {}
};

struct LoginRequest {
    std::string username;
    std::string password;
    
    Json::Value toJson() const {
        Json::Value json;
        json["username"] = username;
        json["password"] = password;
        return json;
    }
    
    static LoginRequest fromJson(const Json::Value& json) {
        LoginRequest req;
        req.username = json["username"].asString();
        req.password = json["password"].asString();
        return req;
    }
};

struct RegisterRequest {
    std::string username;
    std::string password;
    std::string email;
    
    Json::Value toJson() const {
        Json::Value json;
        json["username"] = username;
        json["password"] = password;
        json["email"] = email;
        return json;
    }
    
    static RegisterRequest fromJson(const Json::Value& json) {
        RegisterRequest req;
        req.username = json["username"].asString();
        req.password = json["password"].asString();
        req.email = json["email"].asString();
        return req;
    }
};

struct JoinRoomRequest {
    int room_id;
    
    Json::Value toJson() const {
        Json::Value json;
        json["room_id"] = room_id;
        return json;
    }
    
    static JoinRoomRequest fromJson(const Json::Value& json) {
        JoinRoomRequest req;
        req.room_id = json["room_id"].asInt();
        return req;
    }
};

struct SendMessageRequest {
    int room_id;
    std::string content;
    
    Json::Value toJson() const {
        Json::Value json;
        json["room_id"] = room_id;
        json["content"] = content;
        return json;
    }
    
    static SendMessageRequest fromJson(const Json::Value& json) {
        SendMessageRequest req;
        req.room_id = json["room_id"].asInt();
        req.content = json["content"].asString();
        return req;
    }
};

struct CreateRoomRequest {
    std::string name;
    std::string description;
    int max_users;
    
    Json::Value toJson() const {
        Json::Value json;
        json["name"] = name;
        json["description"] = description;
        json["max_users"] = max_users;
        return json;
    }
    
    static CreateRoomRequest fromJson(const Json::Value& json) {
        CreateRoomRequest req;
        req.name = json["name"].asString();
        req.description = json["description"].asString();
        req.max_users = json["max_users"].asInt();
        return req;
    }
};

struct BaseResponse {
    ResponseCode code;
    std::string message;
    Json::Value data;
    
    Json::Value toJson() const {
        Json::Value json;
        json["code"] = static_cast<int>(code);
        json["message"] = message;
        json["data"] = data;
        return json;
    }
    
    static BaseResponse fromJson(const Json::Value& json) {
        BaseResponse resp;
        resp.code = static_cast<ResponseCode>(json["code"].asInt());
        resp.message = json["message"].asString();
        resp.data = json["data"];
        return resp;
    }
};

struct LoginResponse {
    ResponseCode code;
    std::string message;
    std::string session_id;
    int user_id;
    std::string username;
    std::string email;
    int role;
    
    Json::Value toJson() const {
        Json::Value json;
        json["code"] = static_cast<int>(code);
        json["message"] = message;
        json["session_id"] = session_id;
        json["user_id"] = user_id;
        json["username"] = username;
        json["email"] = email;
        json["role"] = role;
        return json;
    }
    
    static LoginResponse fromJson(const Json::Value& json) {
        LoginResponse resp;
        resp.code = static_cast<ResponseCode>(json["code"].asInt());
        resp.message = json["message"].asString();
        resp.session_id = json["session_id"].asString();
        resp.user_id = json["user_id"].asInt();
        resp.username = json["username"].asString();
        resp.email = json["email"].asString();
        resp.role = json["role"].asInt();
        return resp;
    }
};

struct RoomInfo {
    int id;
    std::string name;
    std::string description;
    int creator_id;
    std::string creator_name;
    int max_users;
    int current_users;
    int status;
    std::string created_time;
    std::string last_activity_time;
    
    Json::Value toJson() const {
        Json::Value json;
        json["id"] = id;
        json["name"] = name;
        json["description"] = description;
        json["creator_id"] = creator_id;
        json["creator_name"] = creator_name;
        json["max_users"] = max_users;
        json["current_users"] = current_users;
        json["status"] = status;
        json["created_time"] = created_time;
        json["last_activity_time"] = last_activity_time;
        return json;
    }
    
    static RoomInfo fromJson(const Json::Value& json) {
        RoomInfo room;
        room.id = json["id"].asInt();
        room.name = json["name"].asString();
        room.description = json["description"].asString();
        room.creator_id = json["creator_id"].asInt();
        room.creator_name = json["creator_name"].asString();
        room.max_users = json["max_users"].asInt();
        room.current_users = json["current_users"].asInt();
        room.status = json["status"].asInt();
        room.created_time = json["created_time"].asString();
        room.last_activity_time = json["last_activity_time"].asString();
        return room;
    }
};

struct RoomListResponse {
    ResponseCode code;
    std::string message;
    std::vector<RoomInfo> rooms;
    
    Json::Value toJson() const {
        Json::Value json;
        json["code"] = static_cast<int>(code);
        json["message"] = message;
        Json::Value roomsArray;
        for (const auto& room : rooms) {
            roomsArray.append(room.toJson());
        }
        json["rooms"] = roomsArray;
        return json;
    }
    
    static RoomListResponse fromJson(const Json::Value& json) {
        RoomListResponse resp;
        resp.code = static_cast<ResponseCode>(json["code"].asInt());
        resp.message = json["message"].asString();
        
        const Json::Value& roomsArray = json["rooms"];
        for (const auto& roomJson : roomsArray) {
            resp.rooms.push_back(RoomInfo::fromJson(roomJson));
        }
        return resp;
    }
};

struct RoomMemberInfo {
    int user_id;
    std::string username;
    std::string email;
    int role;
    std::string join_time;
    
    Json::Value toJson() const {
        Json::Value json;
        json["user_id"] = user_id;
        json["username"] = username;
        json["email"] = email;
        json["role"] = role;
        json["join_time"] = join_time;
        return json;
    }
    
    static RoomMemberInfo fromJson(const Json::Value& json) {
        RoomMemberInfo member;
        member.user_id = json["user_id"].asInt();
        member.username = json["username"].asString();
        member.email = json["email"].asString();
        member.role = json["role"].asInt();
        member.join_time = json["join_time"].asString();
        return member;
    }
};

struct RoomMembersResponse {
    ResponseCode code;
    std::string message;
    int room_id;
    std::vector<RoomMemberInfo> members;
    
    Json::Value toJson() const {
        Json::Value json;
        json["code"] = static_cast<int>(code);
        json["message"] = message;
        json["room_id"] = room_id;
        Json::Value membersArray;
        for (const auto& member : members) {
            membersArray.append(member.toJson());
        }
        json["members"] = membersArray;
        return json;
    }
    
    static RoomMembersResponse fromJson(const Json::Value& json) {
        RoomMembersResponse resp;
        resp.code = static_cast<ResponseCode>(json["code"].asInt());
        resp.message = json["message"].asString();
        resp.room_id = json["room_id"].asInt();
        
        const Json::Value& membersArray = json["members"];
        for (const auto& memberJson : membersArray) {
            resp.members.push_back(RoomMemberInfo::fromJson(memberJson));
        }
        return resp;
    }
};

struct ChatMessagePush {
    int room_id;
    int sender_id;
    std::string sender_name;
    std::string content;
    std::string timestamp;
    
    Json::Value toJson() const {
        Json::Value json;
        json["type"] = static_cast<int>(MessageType::CHAT_MESSAGE_PUSH);
        json["room_id"] = room_id;
        json["sender_id"] = sender_id;
        json["sender_name"] = sender_name;
        json["content"] = content;
        json["timestamp"] = timestamp;
        return json;
    }
    
    static ChatMessagePush fromJson(const Json::Value& json) {
        ChatMessagePush push;
        push.room_id = json["room_id"].asInt();
        push.sender_id = json["sender_id"].asInt();
        push.sender_name = json["sender_name"].asString();
        push.content = json["content"].asString();
        push.timestamp = json["timestamp"].asString();
        return push;
    }
};

struct UserJoinPush {
    int room_id;
    int user_id;
    std::string username;
    std::string email;
    int role;
    std::string join_time;
    
    Json::Value toJson() const {
        Json::Value json;
        json["type"] = static_cast<int>(MessageType::USER_JOIN_PUSH);
        json["room_id"] = room_id;
        json["user_id"] = user_id;
        json["username"] = username;
        json["email"] = email;
        json["role"] = role;
        json["join_time"] = join_time;
        return json;
    }
    
    static UserJoinPush fromJson(const Json::Value& json) {
        UserJoinPush push;
        push.room_id = json["room_id"].asInt();
        push.user_id = json["user_id"].asInt();
        push.username = json["username"].asString();
        push.email = json["email"].asString();
        push.role = json["role"].asInt();
        push.join_time = json["join_time"].asString();
        return push;
    }
};

struct UserLeavePush {
    int room_id;
    int user_id;
    std::string username;
    
    Json::Value toJson() const {
        Json::Value json;
        json["type"] = static_cast<int>(MessageType::USER_LEAVE_PUSH);
        json["room_id"] = room_id;
        json["user_id"] = user_id;
        json["username"] = username;
        return json;
    }
    
    static UserLeavePush fromJson(const Json::Value& json) {
        UserLeavePush push;
        push.room_id = json["room_id"].asInt();
        push.user_id = json["user_id"].asInt();
        push.username = json["username"].asString();
        return push;
    }
};

}