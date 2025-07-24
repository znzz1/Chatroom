#pragma once
#include <string>
#include <vector>
#include "models/Room.h"
#include "utils/QueryResult.h"

class RoomDao {
public:
    virtual ~RoomDao() = default;
    
    virtual QueryResult<Room> createRoom(int adminId, const std::string& name, const std::string& description, int maxUsers = 0) = 0;
    virtual QueryResult<void> deleteRoom(int id) = 0;
    virtual QueryResult<void> setRoomStatus(int room_id, bool is_active) = 0;
    virtual QueryResult<void> setRoomDescription(int room_id, const std::string& description) = 0;
    virtual QueryResult<void> setRoomName(int room_id, const std::string& name) = 0;
    virtual QueryResult<void> setRoomMaxUsers(int room_id, int max_users) = 0;
    virtual QueryResult<std::vector<Room>> getAllRooms() = 0;
    virtual QueryResult<std::vector<Room>> getActiveRooms() = 0;
    virtual QueryResult<Room> getRoomById(int roomId) = 0;
}; 