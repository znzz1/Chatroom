#include "dao/MySqlRoomDao.h"
#include <iostream>

Room MySqlRoomDao::createFromResultSet(const std::vector<std::string>& row) {
    return Room{std::stoi(row[0]), row[1], row[2], std::stoi(row[3]), std::stoi(row[4]), (row[5] == "TRUE"), row[6]};
}

QueryResult<Room> MySqlRoomDao::createRoom(int adminId, const std::string& name, const std::string& description, int maxUsers) {
    auto transactionResult = executeTransaction([&](std::shared_ptr<DatabaseConnection> conn) -> QueryResult<ExecuteResult> {
        auto insertResult = execute(conn, 
            "INSERT INTO rooms (name, description, creator_id, max_users, is_active, created_time) VALUES (?, ?, ?, ?, ?, NOW())",
            name, description, adminId, maxUsers, true);
        
        if (insertResult.isError()) return insertResult;

        auto roomIdResult = execute(conn, "SELECT LAST_INSERT_ID()");
        if (roomIdResult.isError()) return roomIdResult;
        
        int roomId = 0;
        if (roomIdResult.data && std::holds_alternative<std::vector<std::string>>(*roomIdResult.data)) {
            const auto& row = std::get<std::vector<std::string>>(*roomIdResult.data);
            if (!row.empty()) {
                try {
                    roomId = std::stoi(row[0]);
                } catch (...) {
                    return QueryResult<ExecuteResult>::InternalError("Invalid room ID format");
                }
            }
        }
        
        return execute(conn, "SELECT id, name, description, creator_id, max_users, is_active, created_time FROM rooms WHERE id = ?", roomId);
    });
    
    return QueryResult<Room>::convertFrom<Room>(
        transactionResult,
        [this](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}

QueryResult<void> MySqlRoomDao::deleteRoom(int room_id) {
    auto result = execute("DELETE FROM rooms WHERE id = ?", room_id);
    return QueryResult<void>::convertFrom(result);
}

QueryResult<void> MySqlRoomDao::setRoomStatus(int room_id, bool is_active) {
    auto result = execute("UPDATE rooms SET is_active = ? WHERE id = ?", is_active, room_id);
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
    auto result = execute("SELECT id, name, description, creator_id, max_users, is_active, created_time FROM rooms ORDER BY id DESC");
    
    return QueryResult<std::vector<Room>>::convertFromMultiple<std::vector<Room>>(
        result,
        [this](const std::vector<std::vector<std::string>>& rows) {
            std::vector<Room> rooms;
            for (const auto& row : rows) {
                rooms.push_back(createFromResultSet(row));
            }
            return rooms;
        }
    );
}

QueryResult<std::vector<Room>> MySqlRoomDao::getActiveRooms() {
    auto result = execute("SELECT id, name, description, creator_id, max_users, is_active, created_time FROM rooms WHERE is_active = TRUE ORDER BY id DESC");
    
    return QueryResult<std::vector<Room>>::convertFromMultiple<std::vector<Room>>(
        result,
        [this](const std::vector<std::vector<std::string>>& rows) {
            std::vector<Room> rooms;
            for (const auto& row : rows) {
                rooms.push_back(createFromResultSet(row));
            }
            return rooms;
        }
    );
}

QueryResult<Room> MySqlRoomDao::getRoomById(int roomId) {
    auto result = execute("SELECT id, name, description, creator_id, max_users, is_active, created_time FROM rooms WHERE id = ?", roomId);
    return QueryResult<Room>::convertFrom<Room>(
        result,
        [this](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}
