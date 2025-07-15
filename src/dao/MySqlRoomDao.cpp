#include "dao/MySqlRoomDao.h"
#include "database/DatabaseManager.h"
#include <iostream>

Room MySqlRoomDao::createFromResultSet(const std::vector<std::string>& row) {
    Room room;
    if (row.size() >= 7) {
        room.id = std::stoi(row[0]);
        room.name = row[1];
        room.description = row[2];
        room.creator_id = std::stoi(row[3]);
        room.max_users = std::stoi(row[4]);
        room.status = (row[5] == "ACTIVE") ? RoomStatus::ACTIVE : RoomStatus::INACTIVE;
        room.created_time = row[6];
    }
    return room;
}

QueryResult<Room> MySqlRoomDao::createRoom(int adminId, const std::string& name, const std::string& description, int maxUsers) {
    auto transactionResult = executeTransaction([&](std::shared_ptr<DatabaseConnection> conn) -> QueryResult<ExecuteResult> {
        auto insertResult = execute(conn, 
            "INSERT INTO rooms (name, description, creator_id, max_users, status, created_time) VALUES (?, ?, ?, ?, ?, NOW())",
            name, description, adminId, maxUsers, "ACTIVE");
        
        if (insertResult.isError()) return insertResult;

        auto roomIdResult = execute(conn, "SELECT LAST_INSERT_ID()");
        if (roomIdResult.isError()) return roomIdResult;
        int roomId = std::stoi(*roomIdResult.data);
        
        return execute(conn, "SELECT id, name, description, creator_id, max_users, status, created_time FROM rooms WHERE id = ?", roomId);
    });
    
    return QueryResult<Room>::convertFrom(
        transactionResult,
        [](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}

QueryResult<void> MySqlRoomDao::deleteRoom(int room_id) {
    auto result = execute("DELETE FROM rooms WHERE id = ?", room_id);
    return QueryResult<void>::convertFrom(result);
}

QueryResult<void> MySqlRoomDao::setRoomStatus(int room_id, RoomStatus status) {
    std::string statusStr = (status == RoomStatus::ACTIVE) ? "ACTIVE" : "INACTIVE";
    auto result = execute("UPDATE rooms SET status = ? WHERE id = ?", statusStr, room_id);
    return QueryResult<void>::convertFrom(result);
}

QueryResult<void> MySqlRoomDao::setRoomDescription(int room_id, const std::string& description) {
    auto result = execute("UPDATE rooms SET description = ? WHERE id = ?", description, room_id);
    return QueryResult<void>::convertFrom(result);
}

QueryResult<void> MySqlRoomDao::setRoomName(int room_id, const std::string& name) {
    auto result = execute("UPDATE rooms SET name = ? WHERE id = ?", name, room_id);
    return QueryResult<void>::convertFrom(result);
}

QueryResult<void> MySqlRoomDao::setRoomMaxUsers(int room_id, int max_users) {
    auto result = execute("UPDATE rooms SET max_users = ? WHERE id = ?", max_users, room_id);
    return QueryResult<void>::convertFrom(result);
}

QueryResult<std::vector<Room>> MySqlRoomDao::getAllRooms() {
    auto result = execute("SELECT id, name, description, creator_id, max_users, status, created_time FROM rooms ORDER BY id DESC");
    
    return QueryResult<std::vector<Room>>::convertFrom(
        result,
        [](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}

QueryResult<std::vector<Room>> MySqlRoomDao::getActiveRooms() {
    auto result = execute("SELECT id, name, description, creator_id, max_users, status, created_time FROM rooms WHERE status = 'ACTIVE' ORDER BY id DESC");
    
    return QueryResult<std::vector<Room>>::convertFrom(
        result,
        [](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}

QueryResult<Room> MySqlRoomDao::getRoomById(int roomId) {
    auto result = execute("SELECT id, name, description, creator_id, max_users, status, created_time FROM rooms WHERE id = ?", roomId);
    return QueryResult<Room>::convertFrom(
        result,
        [](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}
