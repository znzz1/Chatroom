#pragma once
#include <string>
#include "models/User.h"
#include "utils/QueryResult.h"

class UserDao {
public:
    virtual ~UserDao() = default;
    
    virtual QueryResult<User> createUser(const std::string& name, const std::string& email, const std::string& password_hash, bool is_admin = false) = 0;
    virtual QueryResult<void> changePassword(const std::string& email, const std::string& old_password, const std::string& new_password) = 0;
    virtual QueryResult<void> changeDisplayName(int user_id, const std::string& new_name) = 0;
    virtual QueryResult<User> authenticateUser(const std::string& email, const std::string& password) = 0;
    virtual QueryResult<User> getUserById(int id) = 0;
    virtual QueryResult<User> getUserByEmail(const std::string& email) = 0;
    virtual QueryResult<User> getUserByFullName(const std::string& name, const std::string& discriminator) = 0;
}; 