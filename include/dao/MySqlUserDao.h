#pragma once
#include "UserDao.h"
#include <mysql/mysql.h>
#include <string>
#include <memory>

class MySqlUserDao : public UserDao {
private:
    MYSQL* mysql_;
    std::string host_;
    int port_;
    std::string username_;
    std::string password_;
    std::string database_;
    
    bool connect();
    void disconnect();
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelect(const std::string& query);
    
public:
    MySqlUserDao(const std::string& host, int port, 
                 const std::string& username, const std::string& password,
                 const std::string& database);
    ~MySqlUserDao();
    
    bool createUser(const User& user) override;
    std::optional<User> getUserById(int id) override;
    std::optional<User> getUserByUsername(const std::string& username) override;
    bool updateUser(const User& user) override;
    bool deleteUser(int id) override;
    
    bool setUserOnline(int id, bool online) override;
    std::vector<User> getOnlineUsers() override;
    
    bool authenticateUser(const std::string& username, const std::string& password) override;
    
    int getTotalUserCount() override;
    int getOnlineUserCount() override;
}; 