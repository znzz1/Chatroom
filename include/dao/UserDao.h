#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>

enum class UserRole {
    NORMAL = 1,
    ADMIN = 2
};

struct User {
    int id;
    std::string username;
    std::string password_hash;
    std::string email;
    UserRole role;
    bool is_online;
    std::string last_login_time;
    std::string created_time;
    
    User() : id(0), role(UserRole::NORMAL), is_online(false) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        created_time = std::ctime(&time_t);
        if (!created_time.empty() && created_time.back() == '\n') {
            created_time.pop_back();
        }
    }
    User(const std::string& name, const std::string& pwd, UserRole r = UserRole::NORMAL) 
        : id(0), username(name), password_hash(pwd), role(r), is_online(false) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        created_time = std::ctime(&time_t);
        if (!created_time.empty() && created_time.back() == '\n') {
            created_time.pop_back();
        }
    }
};

class UserDao {
public:
    virtual ~UserDao() = default;
    
    virtual bool createUser(const User& user) = 0;
    virtual std::optional<User> getUserById(int id) = 0;
    virtual std::optional<User> getUserByUsername(const std::string& username) = 0;
    virtual bool updateUser(const User& user) = 0;
    virtual bool deleteUser(int id) = 0;
    
    virtual bool setUserOnline(int id, bool online) = 0;
    virtual std::vector<User> getOnlineUsers() = 0;
    
    virtual bool authenticateUser(const std::string& username, const std::string& password) = 0;
    
    virtual int getTotalUserCount() = 0;
    virtual int getOnlineUserCount() = 0;
}; 