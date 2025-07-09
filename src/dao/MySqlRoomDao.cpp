#include "dao/MySqlRoomDao.h"
#include "dao/DatabaseManager.h"
#include <iostream>
#include <sstream>
#include <cstring>

bool MySqlRoomDao::executeQuery(const std::string& query) {
    return DatabaseManager::getInstance()->executeQuery(query);
}

MYSQL_RES* MySqlRoomDao::executeSelect(const std::string& query) {
    return DatabaseManager::getInstance()->executeSelect(query);
}

bool MySqlRoomDao::createRoom(const Room& room) {
    std::stringstream ss;
    ss << "INSERT INTO rooms (name, description, creator_id, max_users, created_time, last_activity_time) "
       << "VALUES ('" << room.name << "', '" << room.description << "', " 
       << room.creator_id << ", " << room.max_users << ", NOW(), NOW())";
    
    return executeQuery(ss.str());
}

std::optional<Room> MySqlRoomDao::getRoomById(int id) {
    std::string query = "SELECT * FROM rooms WHERE id = " + std::to_string(id);
    MYSQL_RES* result = executeSelect(query);
    if (!result) return std::nullopt;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return std::nullopt;
    }
    
    Room room;
    room.id = std::stoi(row[0]);
    room.name = row[1];
    room.description = row[2] ? row[2] : "";
    room.creator_id = std::stoi(row[3]);
    room.max_users = std::stoi(row[4]);
    room.current_users = std::stoi(row[5]);
    room.status = static_cast<RoomStatus>(std::stoi(row[6]));
    room.created_time = row[7];
    room.last_activity_time = row[8];
    
    mysql_free_result(result);
    return room;
}

std::optional<Room> MySqlRoomDao::getRoomByName(const std::string& name) {
    std::string query = "SELECT * FROM rooms WHERE name = '" + name + "'";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return std::nullopt;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return std::nullopt;
    }
    
    Room room;
    room.id = std::stoi(row[0]);
    room.name = row[1];
    room.description = row[2] ? row[2] : "";
    room.creator_id = std::stoi(row[3]);
    room.max_users = std::stoi(row[4]);
    room.current_users = std::stoi(row[5]);
    room.status = static_cast<RoomStatus>(std::stoi(row[6]));
    room.created_time = row[7];
    room.last_activity_time = row[8];
    
    mysql_free_result(result);
    return room;
}

bool MySqlRoomDao::updateRoom(const Room& room) {
    std::stringstream ss;
    ss << "UPDATE rooms SET name = '" << room.name << "', description = '" << room.description 
       << "', max_users = " << room.max_users << ", current_users = " << room.current_users
       << ", status = " << static_cast<int>(room.status) << ", last_activity_time = NOW() "
       << "WHERE id = " << room.id;
    
    return executeQuery(ss.str());
}

bool MySqlRoomDao::deleteRoom(int id) {
    std::string query = "DELETE FROM rooms WHERE id = " + std::to_string(id);
    return executeQuery(query);
}

std::vector<Room> MySqlRoomDao::getAllRooms() {
    std::vector<Room> rooms;
    std::string query = "SELECT * FROM rooms ORDER BY last_activity_time DESC";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return rooms;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Room room;
        room.id = std::stoi(row[0]);
        room.name = row[1];
        room.description = row[2] ? row[2] : "";
        room.creator_id = std::stoi(row[3]);
        room.max_users = std::stoi(row[4]);
        room.current_users = std::stoi(row[5]);
        room.status = static_cast<RoomStatus>(std::stoi(row[6]));
        room.created_time = row[7];
        room.last_activity_time = row[8];
        rooms.push_back(room);
    }
    
    mysql_free_result(result);
    return rooms;
}

std::vector<Room> MySqlRoomDao::getActiveRooms() {
    std::vector<Room> rooms;
    std::string query = "SELECT * FROM rooms WHERE status = 1 ORDER BY last_activity_time DESC";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return rooms;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Room room;
        room.id = std::stoi(row[0]);
        room.name = row[1];
        room.description = row[2] ? row[2] : "";
        room.creator_id = std::stoi(row[3]);
        room.max_users = std::stoi(row[4]);
        room.current_users = std::stoi(row[5]);
        room.status = static_cast<RoomStatus>(std::stoi(row[6]));
        room.created_time = row[7];
        room.last_activity_time = row[8];
        rooms.push_back(room);
    }
    
    mysql_free_result(result);
    return rooms;
}

std::vector<Room> MySqlRoomDao::getRoomsByCreator(int creator_id) {
    std::vector<Room> rooms;
    std::string query = "SELECT * FROM rooms WHERE creator_id = " + std::to_string(creator_id) + " ORDER BY created_time DESC";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return rooms;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Room room;
        room.id = std::stoi(row[0]);
        room.name = row[1];
        room.description = row[2] ? row[2] : "";
        room.creator_id = std::stoi(row[3]);
        room.max_users = std::stoi(row[4]);
        room.current_users = std::stoi(row[5]);
        room.status = static_cast<RoomStatus>(std::stoi(row[6]));
        room.created_time = row[7];
        room.last_activity_time = row[8];
        rooms.push_back(room);
    }
    
    mysql_free_result(result);
    return rooms;
}

bool MySqlRoomDao::addUserToRoom(int room_id, int user_id) {
    if (isUserInRoom(room_id, user_id)) {
        return false;
    }
    
    std::stringstream ss;
    ss << "INSERT INTO room_members (room_id, user_id, name, discriminator, email, role, join_time) "
       << "SELECT " << room_id << ", " << user_id << ", name, discriminator, email, role, NOW() "
       << "FROM users WHERE id = " << user_id;
    
    if (!executeQuery(ss.str())) return false;
    
    updateRoomUserCount(room_id, -1);
    return true;
}

bool MySqlRoomDao::removeUserFromRoom(int room_id, int user_id) {
    std::string query = "DELETE FROM room_members WHERE room_id = " + std::to_string(room_id) 
                       + " AND user_id = " + std::to_string(user_id);
    
    if (!executeQuery(query)) return false;
    
    updateRoomUserCount(room_id, -1);
    return true;
}

std::vector<int> MySqlRoomDao::getRoomMembers(int room_id) {
    std::vector<int> members;
    std::string query = "SELECT user_id FROM room_members WHERE room_id = " + std::to_string(room_id);
    MYSQL_RES* result = executeSelect(query);
    if (!result) return members;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        members.push_back(std::stoi(row[0]));
    }
    
    mysql_free_result(result);
    return members;
}

std::vector<RoomMember> MySqlRoomDao::getRoomMemberDetails(int room_id) {
    std::vector<RoomMember> members;
    std::string query = "SELECT * FROM room_members WHERE room_id = " + std::to_string(room_id) + " ORDER BY join_time";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return members;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        RoomMember member;
        member.room_id = std::stoi(row[0]);
        member.user_id = std::stoi(row[1]);
        member.name = row[2];
        member.discriminator = row[3];
        member.email = row[4];
        member.role = static_cast<UserRole>(std::stoi(row[5]));
        member.join_time = row[6];
        members.push_back(member);
    }
    
    mysql_free_result(result);
    return members;
}

bool MySqlRoomDao::isUserInRoom(int room_id, int user_id) {
    std::string query = "SELECT COUNT(*) FROM room_members WHERE room_id = " + std::to_string(room_id) 
                       + " AND user_id = " + std::to_string(user_id);
    MYSQL_RES* result = executeSelect(query);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    bool inRoom = row && std::stoi(row[0]) > 0;
    mysql_free_result(result);
    return inRoom;
}

bool MySqlRoomDao::updateRoomUserCount(int room_id, int count) {
    if (count == -1) {
        std::string query = "UPDATE rooms SET current_users = (SELECT COUNT(*) FROM room_members WHERE room_id = " 
                           + std::to_string(room_id) + ") WHERE id = " + std::to_string(room_id);
        return executeQuery(query);
    } else {
        std::string query = "UPDATE rooms SET current_users = " + std::to_string(count) 
                           + " WHERE id = " + std::to_string(room_id);
        return executeQuery(query);
    }
}

bool MySqlRoomDao::updateRoomStatus(int room_id, RoomStatus status) {
    std::string query = "UPDATE rooms SET status = " + std::to_string(static_cast<int>(status)) 
                       + " WHERE id = " + std::to_string(room_id);
    return executeQuery(query);
}

bool MySqlRoomDao::updateRoomLastActivity(int room_id) {
    std::string query = "UPDATE rooms SET last_activity_time = NOW() WHERE id = " + std::to_string(room_id);
    return executeQuery(query);
}

int MySqlRoomDao::getTotalRoomCount() {
    std::string query = "SELECT COUNT(*) FROM rooms";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return 0;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count;
}

int MySqlRoomDao::getActiveRoomCount() {
    std::string query = "SELECT COUNT(*) FROM rooms WHERE status = 1";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return 0;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count;
}

int MySqlRoomDao::getRoomMemberCount(int room_id) {
    std::string query = "SELECT COUNT(*) FROM room_members WHERE room_id = " + std::to_string(room_id);
    MYSQL_RES* result = executeSelect(query);
    if (!result) return 0;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count;
}

std::vector<int> MySqlRoomDao::getUserRooms(int user_id) {
    std::vector<int> rooms;
    std::string query = "SELECT room_id FROM room_members WHERE user_id = " + std::to_string(user_id);
    MYSQL_RES* result = executeSelect(query);
    if (!result) return rooms;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        rooms.push_back(std::stoi(row[0]));
    }
    
    mysql_free_result(result);
    return rooms;
} 