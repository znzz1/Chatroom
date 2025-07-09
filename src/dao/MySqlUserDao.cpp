#include "dao/MySqlUserDao.h"
#include "dao/DatabaseManager.h"
#include "utils/PasswordHasher.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <random>
#include <set>

bool MySqlUserDao::executeQuery(const std::string& query) {
    return DatabaseManager::getInstance()->executeQuery(query);
}

MYSQL_RES* MySqlUserDao::executeSelect(const std::string& query) {
    return DatabaseManager::getInstance()->executeSelect(query);
}

bool MySqlUserDao::createUser(const User& user) {
    std::stringstream ss;
    ss << "INSERT INTO users (discriminator, name, password_hash, email, role, is_online, created_time) "
       << "VALUES ('" << user.discriminator << "', '" << user.name << "', '" 
       << user.password_hash << "', '" << user.email << "', " << static_cast<int>(user.role) 
       << ", " << (user.is_online ? 1 : 0) << ", NOW())";
    
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
    user.discriminator = row[1];
    user.name = row[2];
    user.password_hash = row[3];
    user.email = row[4];
    user.role = static_cast<UserRole>(std::stoi(row[5]));
    user.is_online = std::stoi(row[6]) != 0;
    user.created_time = row[7];
    
    mysql_free_result(result);
    return user;
}

std::optional<User> MySqlUserDao::getUserByEmail(const std::string& email) {
    std::string query = "SELECT * FROM users WHERE email = '" + email + "'";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return std::nullopt;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return std::nullopt;
    }
    
    User user;
    user.id = std::stoi(row[0]);
    user.discriminator = row[1];
    user.name = row[2];
    user.password_hash = row[3];
    user.email = row[4];
    user.role = static_cast<UserRole>(std::stoi(row[5]));
    user.is_online = std::stoi(row[6]) != 0;
    user.created_time = row[7];
    
    mysql_free_result(result);
    return user;
}

std::optional<User> MySqlUserDao::getUserByFullName(const std::string& name, const std::string& discriminator) {
    std::string query = "SELECT * FROM users WHERE name = '" + name + "' AND discriminator = '" + discriminator + "'";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return std::nullopt;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return std::nullopt;
    }
    
    User user;
    user.id = std::stoi(row[0]);
    user.discriminator = row[1];
    user.name = row[2];
    user.password_hash = row[3];
    user.email = row[4];
    user.role = static_cast<UserRole>(std::stoi(row[5]));
    user.is_online = std::stoi(row[6]) != 0;
    user.created_time = row[7];
    
    mysql_free_result(result);
    return user;
}

bool MySqlUserDao::updateUser(const User& user) {
    std::stringstream ss;
    ss << "UPDATE users SET discriminator = '" << user.discriminator << "', name = '" 
       << user.name << "', password_hash = '" << user.password_hash 
       << "', email = '" << user.email << "', role = " << static_cast<int>(user.role) 
       << ", is_online = " << (user.is_online ? 1 : 0) << " "
       << "WHERE id = " << user.id;
    
    return executeQuery(ss.str());
}

bool MySqlUserDao::deleteUser(int id) {
    std::string query = "DELETE FROM users WHERE id = " + std::to_string(id);
    return executeQuery(query);
}

bool MySqlUserDao::setUserOnline(int id, bool online) {
    std::stringstream ss;
    ss << "UPDATE users SET is_online = " << (online ? 1 : 0) << " WHERE id = " << id;
    return executeQuery(ss.str());
}

std::vector<User> MySqlUserDao::getOnlineUsers() {
    std::vector<User> users;
    std::string query = "SELECT * FROM users WHERE is_online = 1 ORDER BY created_time DESC";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return users;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        User user;
        user.id = std::stoi(row[0]);
        user.discriminator = row[1];
        user.name = row[2];
        user.password_hash = row[3];
        user.email = row[4];
        user.role = static_cast<UserRole>(std::stoi(row[5]));
        user.is_online = std::stoi(row[6]) != 0;
        user.created_time = row[7];
        users.push_back(user);
    }
    
    mysql_free_result(result);
    return users;
}

bool MySqlUserDao::authenticateUser(const std::string& email, const std::string& password) {
    // 先获取用户的密码哈希
    std::string query = "SELECT password_hash FROM users WHERE email = '" + email + "'";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return false;
    }
    
    std::string storedHash = row[0];
    mysql_free_result(result);
    
    // 验证密码
    return PasswordHasher::verifyPasswordWithFullHash(password, storedHash);
}

bool MySqlUserDao::updateName(int user_id, const std::string& new_name) {
    // 生成新的discriminator
    std::string new_discriminator = generateDiscriminator(new_name);
    
    std::stringstream ss;
    ss << "UPDATE users SET name = '" << new_name << "', discriminator = '" 
       << new_discriminator << "' WHERE id = " << user_id;
    
    return executeQuery(ss.str());
}

std::string MySqlUserDao::getNameById(int user_id) {
    std::string query = "SELECT name FROM users WHERE id = " + std::to_string(user_id);
    MYSQL_RES* result = executeSelect(query);
    if (!result) return "";
    
    MYSQL_ROW row = mysql_fetch_row(result);
    std::string name = row ? row[0] : "";
    mysql_free_result(result);
    return name;
}

std::string MySqlUserDao::getFullNameById(int user_id) {
    std::string query = "SELECT name, discriminator FROM users WHERE id = " + std::to_string(user_id);
    MYSQL_RES* result = executeSelect(query);
    if (!result) return "";
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return "";
    }
    
    std::string full_name = std::string(row[0]) + "#" + std::string(row[1]);
    mysql_free_result(result);
    return full_name;
}

bool MySqlUserDao::isEmailExists(const std::string& email) {
    std::string query = "SELECT COUNT(*) FROM users WHERE email = '" + email + "'";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    bool exists = row && std::stoi(row[0]) > 0;
    mysql_free_result(result);
    return exists;
}

bool MySqlUserDao::isNameDiscriminatorExists(const std::string& name, const std::string& discriminator) {
    std::string query = "SELECT COUNT(*) FROM users WHERE name = '" + name + "' AND discriminator = '" + discriminator + "'";
    MYSQL_RES* result = executeSelect(query);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    bool exists = row && std::stoi(row[0]) > 0;
    mysql_free_result(result);
    return exists;
}

std::string MySqlUserDao::generateDiscriminator(const std::string& name) {
    // 获取该name已使用的discriminator
    std::string query = "SELECT discriminator FROM users WHERE name = '" + name + "' ORDER BY discriminator";
    MYSQL_RES* result = executeSelect(query);
    
    std::set<std::string> used_discriminators;
    if (result) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            used_discriminators.insert(row[0]);
        }
        mysql_free_result(result);
    }
    
    // 如果已经使用了9999个discriminator，抛出异常
    if (used_discriminators.size() >= 10000) {
        throw std::runtime_error("Too many users with the same name: " + name);
    }
    
    // 生成4位随机数字，确保不重复
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 9999);
    
    std::string discriminator;
    int attempts = 0;
    const int max_attempts = 1000; // 防止无限循环
    
    do {
        int num = dis(gen);
        discriminator = std::to_string(num);
        // 补齐4位
        while (discriminator.length() < 4) {
            discriminator = "0" + discriminator;
        }
        attempts++;
        
        if (attempts >= max_attempts) {
            throw std::runtime_error("Failed to generate unique discriminator for name: " + name);
        }
    } while (used_discriminators.find(discriminator) != used_discriminators.end());
    
    return discriminator;
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