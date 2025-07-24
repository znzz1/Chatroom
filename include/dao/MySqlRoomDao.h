#pragma once
#include "dao/RoomDao.h"
#include "dao/SqlDao.h"
#include <memory>

class MySqlRoomDao : public RoomDao, public SqlDao<Room> {
public:
    MySqlRoomDao() = default;
    ~MySqlRoomDao() = default;
    
    QueryResult<Room> createRoom(int adminId, const std::string& name, const std::string& description, int maxUsers = 0) override;
    QueryResult<void> deleteRoom(int id) override;
    
    QueryResult<void> setRoomStatus(int room_id, bool is_active) override;
    QueryResult<void> setRoomDescription(int room_id, const std::string& description) override;
    QueryResult<void> setRoomName(int room_id, const std::string& name) override;
    QueryResult<void> setRoomMaxUsers(int room_id, int max_users) override;
    
    QueryResult<std::vector<Room>> getAllRooms() override;
    QueryResult<std::vector<Room>> getActiveRooms() override;
    QueryResult<Room> getRoomById(int roomId) override;

protected:
    Room createFromResultSet(const std::vector<std::string>& row) override;
}; 