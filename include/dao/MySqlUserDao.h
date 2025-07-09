#pragma once
#include "UserDao.h"
#include <mysql/mysql.h>
#include <string>
#include <memory>

class MySqlUserDao : public UserDao {
private:
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelect(const std::string& query);
    
public:
    MySqlUserDao() = default;
    ~MySqlUserDao() = default;
    
    bool createUser(const User& user) override;
    std::optional<User> getUserById(int id) override;
    std::optional<User> getUserByEmail(const std::string& email) override;
    std::optional<User> getUserByFullName(const std::string& name, const std::string& discriminator) override;
    bool updateUser(const User& user) override;
    bool deleteUser(int id) override;
    
    bool setUserOnline(int id, bool online) override;
    std::vector<User> getOnlineUsers() override;
    
    bool authenticateUser(const std::string& email, const std::string& password) override;
    
    bool updateName(int user_id, const std::string& new_name) override;
    std::string getNameById(int user_id) override;
    std::string getFullNameById(int user_id) override;
    
    bool isEmailExists(const std::string& email) override;
    bool isNameDiscriminatorExists(const std::string& name, const std::string& discriminator) override;
    
    std::string generateDiscriminator(const std::string& name) override;
    
    int getTotalUserCount() override;
    int getOnlineUserCount() override;
}; 