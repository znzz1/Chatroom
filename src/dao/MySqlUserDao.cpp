#include "dao/MySqlUserDao.h"
#include "dao/DatabaseManager.h"
#include <iostream>
#include <sstream>
#include <cstring>

bool MySqlUserDao::executeQuery(const std::string& query) {
    return DatabaseManager::getInstance()->executeQuery(query);
}

MYSQL_RES* MySqlUserDao::executeSelect(const std::string& query) {
    return DatabaseManager::getInstance()->executeSelect(query);
}

bool MySqlUserDao::createUser(const User& user) {
    std::stringstream ss;
    ss << "INSERT INTO users (username, password_hash, email, role, created_time) "
       << "VALUES ('" << user.username << "', '" << user.password_hash << "', '" 
       << user.email << "', " << static_cast<int>(user.role) << ", NOW())";
    
    return executeQuery(ss.str());
}

std::optional<User> MySqlUserDao::getUserById(int id) {
    std::string query = "SELECT * FROM users WHERE id = " + std::to_string(id);
    MYSQL_RES* result = executeSelect(query);
    if (!result) return std::nullopt;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return std::nullopt;
    }
    
    User user;
    user.id = std::stoi(row[0]);
    user.username = row[1];
    user.password_hash = row[2];
    user.email = row[3];
    user.role = static_cast<UserRole>(std::stoi(row[4]));
    user.is_online = std::stoi(row[5]) != 0;
    user.last_login_time = row[6] ? row[6] : "";
    user.created_time = row[7];
    
    mysql_free_result(result);
    return user;
}

std::optional<User> MySqlUserDao::getUserByUsername(const std::string& username) {
    std::string query = "SELECT * FROM users WHERE username = '" + username + "'";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return std::nullopt;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return std::nullopt;
    }
    
    User user;
    user.id = std::stoi(row[0]);
    user.username = row[1];
    user.password_hash = row[2];
    user.email = row[3];
    user.role = static_cast<UserRole>(std::stoi(row[4]));
    user.is_online = std::stoi(row[5]) != 0;
    user.last_login_time = row[6] ? row[6] : "";
    user.created_time = row[7];
    
    mysql_free_result(result);
    return user;
}

bool MySqlUserDao::updateUser(const User& user) {
    std::stringstream ss;
    ss << "UPDATE users SET username = '" << user.username << "', password_hash = '" 
       << user.password_hash << "', email = '" << user.email << "', role = " 
       << static_cast<int>(user.role) << ", is_online = " << (user.is_online ? 1 : 0)
       << ", last_login_time = '" << user.last_login_time << "' "
       << "WHERE id = " << user.id;
    
    return executeQuery(ss.str());
}

bool MySqlUserDao::deleteUser(int id) {
    std::string query = "DELETE FROM users WHERE id = " + std::to_string(id);
    return executeQuery(query);
}

bool MySqlUserDao::setUserOnline(int id, bool online) {
    std::stringstream ss;
    ss << "UPDATE users SET is_online = " << (online ? 1 : 0);
    if (online) {
        ss << ", last_login_time = NOW()";
    }
    ss << " WHERE id = " << id;
    
    return executeQuery(ss.str());
}

std::vector<User> MySqlUserDao::getOnlineUsers() {
    std::vector<User> users;
    std::string query = "SELECT * FROM users WHERE is_online = 1 ORDER BY last_login_time DESC";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return users;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        User user;
        user.id = std::stoi(row[0]);
        user.username = row[1];
        user.password_hash = row[2];
        user.email = row[3];
        user.role = static_cast<UserRole>(std::stoi(row[4]));
        user.is_online = std::stoi(row[5]) != 0;
        user.last_login_time = row[6] ? row[6] : "";
        user.created_time = row[7];
        users.push_back(user);
    }
    
    mysql_free_result(result);
    return users;
}

bool MySqlUserDao::authenticateUser(const std::string& username, const std::string& password) {
    std::string query = "SELECT COUNT(*) FROM users WHERE username = '" + username 
                       + "' AND password_hash = '" + password + "'";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    bool authenticated = row && std::stoi(row[0]) > 0;
    mysql_free_result(result);
    return authenticated;
}

int MySqlUserDao::getTotalUserCount() {
    std::string query = "SELECT COUNT(*) FROM users";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return 0;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count;
}

int MySqlUserDao::getOnlineUserCount() {
    std::string query = "SELECT COUNT(*) FROM users WHERE is_online = 1";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return 0;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? std::stoi(row[0]) : 0;
    mysql_free_result(result);
    return count;
} 