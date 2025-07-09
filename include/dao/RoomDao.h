#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>

enum class RoomStatus {
    ACTIVE = 1,
    INACTIVE = 2,
    FULL = 3
};

struct Room {
    int id;
    std::string name;
    std::string description;
    int creator_id;
    int max_users;
    int current_users;
    RoomStatus status;
    std::string created_time;
    std::string last_activity_time;
    
    Room() : id(0), creator_id(0), max_users(0), current_users(0), status(RoomStatus::ACTIVE) {}
    Room(const std::string& room_name, const std::string& desc, int creator, int max = 0)
        : id(0), name(room_name), description(desc), creator_id(creator), 
          max_users(max), current_users(0), status(RoomStatus::ACTIVE) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        created_time = std::ctime(&time_t);
        if (!created_time.empty() && created_time.back() == '\n') {
            created_time.pop_back();
        }
        last_activity_time = created_time;
    }
};

struct RoomMember {
    int room_id;
    int user_id;
    std::string username;
    std::string email;
    UserRole role;
    std::string join_time;
    
    RoomMember() : room_id(0), user_id(0), role(UserRole::NORMAL) {}
    RoomMember(int room, int user, const std::string& name, const std::string& mail, UserRole r = UserRole::NORMAL) 
        : room_id(room), user_id(user), username(name), email(mail), role(r) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        join_time = std::ctime(&time_t);
        if (!join_time.empty() && join_time.back() == '\n') {
            join_time.pop_back();
        }
    }
};

class RoomDao {
public:
    virtual ~RoomDao() = default;
    
    virtual bool createRoom(const Room& room) = 0;
    virtual std::optional<Room> getRoomById(int id) = 0;
    virtual std::optional<Room> getRoomByName(const std::string& name) = 0;
    virtual bool updateRoom(const Room& room) = 0;
    virtual bool deleteRoom(int id) = 0;
    
    virtual std::vector<Room> getAllRooms() = 0;
    virtual std::vector<Room> getActiveRooms() = 0;
    virtual std::vector<Room> getRoomsByCreator(int creator_id) = 0;
    
    virtual bool addUserToRoom(int room_id, int user_id) = 0;
    virtual bool removeUserFromRoom(int room_id, int user_id) = 0;
    virtual std::vector<int> getRoomMembers(int room_id) = 0;
    virtual std::vector<RoomMember> getRoomMemberDetails(int room_id) = 0;
    virtual bool isUserInRoom(int room_id, int user_id) = 0;
    
    virtual bool updateRoomUserCount(int room_id, int count) = 0;
    virtual bool updateRoomStatus(int room_id, RoomStatus status) = 0;
    virtual bool updateRoomLastActivity(int room_id) = 0;
    
    virtual int getTotalRoomCount() = 0;
    virtual int getActiveRoomCount() = 0;
    virtual int getRoomMemberCount(int room_id) = 0;
    virtual std::vector<int> getUserRooms(int user_id) = 0;
}; 