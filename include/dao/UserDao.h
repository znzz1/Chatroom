#pragma once
#include <string>
#include <vector>
#include <memory>
#include "models/User.h"
#include "models/Enums.h"
#include "utils/QueryResult.h"

class UserDao {
public:
    virtual ~UserDao() = default;
    
    virtual QueryResult<User> createUser(const std::string& name, const std::string& email, 
                                          const std::string& password_hash, UserRole role = UserRole::NORMAL) = 0;

    virtual QueryResult<void> changePassword(int user_id, const std::string& old_password, const std::string& new_password) = 0;
    virtual QueryResult<void> changeDisplayName(int user_id, const std::string& new_name) = 0;

    virtual QueryResult<User> authenticateUser(const std::string& email, const std::string& password) = 0;
    
    virtual QueryResult<User> getUserById(int id) = 0;
    virtual QueryResult<User> getUserByEmail(const std::string& email) = 0;
    virtual QueryResult<User> getUserByFullName(const std::string& name, const std::string& discriminator) = 0;

protected:
    virtual std::string generateUniqueDiscriminator(std::shared_ptr<DatabaseConnection> conn, const std::string& name) = 0;
}; 