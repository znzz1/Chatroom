#pragma once
#include "dao/UserDao.h"
#include "dao/SqlDao.h"
#include <memory>

class MySqlUserDao : public UserDao, public SqlDao<User> {
public:
    MySqlUserDao() = default;
    ~MySqlUserDao() = default;
    
    QueryResult<User> createUser(const std::string& name, const std::string& email, const std::string& password_hash, bool is_admin = false) override;
    
    QueryResult<void> changePassword(const std::string& email, const std::string& old_password, const std::string& new_password) override;
    QueryResult<void> changeDisplayName(int user_id, const std::string& new_name) override;
    
    QueryResult<User> authenticateUser(const std::string& email, const std::string& password) override;
    
    QueryResult<User> getUserById(int id) override;
    QueryResult<User> getUserByEmail(const std::string& email) override;
    QueryResult<User> getUserByFullName(const std::string& name, const std::string& discriminator) override;
        
protected:
    std::string generateUniqueDiscriminator(std::shared_ptr<DatabaseConnection> conn, const std::string& name);
    User createFromResultSet(const std::vector<std::string>& row) override;
}; 