#pragma once
#include "RoomDao.h"
#include <mysql/mysql.h>
#include <string>
#include <memory>

class MySqlRoomDao : public RoomDao {
private:
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelect(const std::string& query);
    
public:
    MySqlRoomDao() = default;
    ~MySqlRoomDao() = default;
    
    bool createRoom(const Room& room) override;
    std::optional<Room> getRoomById(int id) override;
    std::optional<Room> getRoomByName(const std::string& name) override;
    bool updateRoom(const Room& room) override;
    bool deleteRoom(int id) override;
    
    std::vector<Room> getAllRooms() override;
    std::vector<Room> getActiveRooms() override;
    std::vector<Room> getRoomsByCreator(int creator_id) override;
    
    bool addUserToRoom(int room_id, int user_id) override;
    bool removeUserFromRoom(int room_id, int user_id) override;
    std::vector<int> getRoomMembers(int room_id) override;
    std::vector<RoomMember> getRoomMemberDetails(int room_id) override;
    bool isUserInRoom(int room_id, int user_id) override;
    
    bool updateRoomUserCount(int room_id, int count) override;
    bool updateRoomStatus(int room_id, RoomStatus status) override;
    bool updateRoomLastActivity(int room_id) override;
    
    int getTotalRoomCount() override;
    int getActiveRoomCount() override;
    int getRoomMemberCount(int room_id) override;
    std::vector<int> getUserRooms(int user_id) override;
}; 