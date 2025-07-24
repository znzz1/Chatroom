#include "dao/MySqlUserDao.h"
#include "utils/PasswordHasher.h"
#include <unordered_set>
#include <sstream>
#include <iomanip>
#include <random>

std::string MySqlUserDao::generateUniqueDiscriminator(std::shared_ptr<DatabaseConnection> conn, const std::string& name) {
    auto result = execute(conn, "SELECT discriminator FROM users WHERE name = ?", name);
    if (result.isConnectionError()) return "0";
    if (result.isInternalError()) return "1";
    
    std::unordered_set<std::string> used;
    if (result.data && std::holds_alternative<std::vector<std::vector<std::string>>>(*result.data)) {
        const auto& rows = std::get<std::vector<std::vector<std::string>>>(*result.data);
        for (const auto& row : rows) {
            if (!row.empty()) used.insert(row[0]);
        }
    } else if (result.data && std::holds_alternative<std::vector<std::string>>(*result.data)) {
        const auto& row = std::get<std::vector<std::string>>(*result.data);
        if (!row.empty()) used.insert(row[0]);
    }

    if (used.size() == 10000) {
        return "2";
    }

    std::string discriminator;
    if (used.size() < 9900) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 9999);
        for (int attempt = 0; attempt < 50; ++attempt) {
            int number = dis(gen);
            std::ostringstream oss;
            oss << std::setw(4) << std::setfill('0') << number;
            discriminator = oss.str();
            if (used.count(discriminator) == 0) {
                return discriminator;
            }
        }
    }

    for (int i = 0; i < 10000; ++i) {
        std::ostringstream oss;
        oss << std::setw(4) << std::setfill('0') << i;
        discriminator = oss.str();
        if (used.count(discriminator) == 0) {
            return discriminator;
        }
    }

    return "2";
}

User MySqlUserDao::createFromResultSet(const std::vector<std::string>& row) { 
    return User{std::stoi(row[0]), row[1], row[2], row[3], (row[4] == "TRUE"), row[5]};
}

QueryResult<User> MySqlUserDao::createUser(const std::string& name, const std::string& email, const std::string& password_hash, bool is_admin) {
    QueryResult<ExecuteResult> transactionResult = executeTransaction([&](std::shared_ptr<DatabaseConnection> conn) -> QueryResult<ExecuteResult> {
        auto emailExistsResult = execute(conn, "SELECT COUNT(*) FROM users WHERE email = ?", email);
        if (emailExistsResult.isError()) return emailExistsResult;
        if (emailExistsResult.isSuccess()) return QueryResult<ExecuteResult>::NotFound("0");
        
        std::string uniqueDiscriminator = generateUniqueDiscriminator(conn, name);
        if (uniqueDiscriminator == "0") return QueryResult<ExecuteResult>::ConnectionError();
        if (uniqueDiscriminator == "1") return QueryResult<ExecuteResult>::InternalError();
        if (uniqueDiscriminator == "2") return QueryResult<ExecuteResult>::NotFound("1");
        
        auto insertUserResult = execute(conn, "INSERT INTO users (discriminator, name, email, password_hash, is_admin, created_time) VALUES (?, ?, ?, ?, ?, NOW())", 
                                    uniqueDiscriminator, name, email, password_hash, is_admin ? "TRUE" : "FALSE");
        if (insertUserResult.isError()) return insertUserResult;
        
        auto userIdResult = execute(conn, "SELECT LAST_INSERT_ID()");
        if (userIdResult.isError()) return userIdResult;
        
        auto extractUserId = [](const ExecuteResult& result) -> std::optional<int> {
            if (std::holds_alternative<std::vector<std::string>>(result)) {
                const auto& row = std::get<std::vector<std::string>>(result);
                if (!row.empty()) {
                    try {
                        return std::stoi(row[0]);
                    } catch (...) {
                        return std::nullopt;
                    }
                }
            }
            return std::nullopt;
        };
        if (!userIdResult.data) {
            return QueryResult<ExecuteResult>::InternalError("No userId data");
        }
        auto idOpt = extractUserId(*userIdResult.data);
        if (!idOpt) {
            return QueryResult<ExecuteResult>::InternalError("No userId returned or invalid format");
        }
        int userId = *idOpt;
        return execute(conn, "SELECT id, discriminator, name, email, is_admin, created_time FROM users WHERE id = ?", userId);
    });
    
    return QueryResult<User>::convertFrom<User>(
        transactionResult,
        [this](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}

QueryResult<void> MySqlUserDao::changePassword(const std::string& email, const std::string& old_password, const std::string& new_password) {
    QueryResult<ExecuteResult> transactionResult = executeTransaction([&](std::shared_ptr<DatabaseConnection> conn) -> QueryResult<ExecuteResult> {
        auto passwordResult = execute(conn, "SELECT password_hash FROM users WHERE email = ?", email);
        if (passwordResult.isError()) return passwordResult;
        if (passwordResult.isNotFound()) return QueryResult<ExecuteResult>::NotFound("0");
        
        auto& data = std::get<std::vector<std::string>>(passwordResult.data.value());
        bool oldPasswordCorrect = PasswordHasher::verifyPasswordWithFullHash(old_password, data[0]);
        if (!oldPasswordCorrect) return QueryResult<ExecuteResult>::NotFound("2");
        
        std::string new_password_hash = PasswordHasher::hashPasswordWithSalt(new_password);
        auto updateResult = execute(conn, "UPDATE users SET password_hash = ? WHERE email = ?", new_password_hash, email);
        if (updateResult.isError()) return updateResult;
        
        return QueryResult<ExecuteResult>::Success(std::monostate{});
    });
    
    return QueryResult<void>::convertFrom(transactionResult);
}

QueryResult<void> MySqlUserDao::changeDisplayName(int user_id, const std::string& new_name) {
    QueryResult<ExecuteResult> transactionResult = executeTransaction([&](std::shared_ptr<DatabaseConnection> conn) -> QueryResult<ExecuteResult> {
        std::string new_discriminator = generateUniqueDiscriminator(conn, new_name);
        if (new_discriminator == "0") return QueryResult<ExecuteResult>::ConnectionError();
        if (new_discriminator == "1") return QueryResult<ExecuteResult>::InternalError();
        if (new_discriminator == "2") return QueryResult<ExecuteResult>::NotFound("1");
        
        auto update_result = execute(conn, "UPDATE users SET name = ?, discriminator = ? WHERE id = ?", 
                                   new_name, new_discriminator, user_id);
        if (update_result.isError()) return update_result;
        
        return QueryResult<ExecuteResult>::Success(std::monostate{});
    });
    
    return QueryResult<void>::convertFrom(transactionResult);
}

QueryResult<User> MySqlUserDao::authenticateUser(const std::string& email, const std::string& password) {
    QueryResult<ExecuteResult> transactionResult = executeTransaction([&](std::shared_ptr<DatabaseConnection> conn) -> QueryResult<ExecuteResult> {
        auto userResult = execute(conn, 
            "SELECT id, discriminator, name, email, is_admin, created_time, password_hash "
            "FROM users WHERE email = ?", 
            email);
        
        if (userResult.isError()) return userResult;
        if (userResult.isNotFound()) return QueryResult<ExecuteResult>::NotFound("0");
        
        auto& data = std::get<std::vector<std::string>>(userResult.data.value());
        bool passwordCorrect = PasswordHasher::verifyPasswordWithFullHash(password, data[6]);
        if (!passwordCorrect) return QueryResult<ExecuteResult>::NotFound("2");
        
        return userResult;
    });
    
    return QueryResult<User>::convertFrom<User>(
        transactionResult,
        [this](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}

QueryResult<User> MySqlUserDao::getUserById(int id) {
    QueryResult<ExecuteResult> execResult = execute(
        "SELECT id, discriminator, name, email, is_admin, created_time "
        "FROM users WHERE id = ?",
        id
    );
    
    return QueryResult<User>::convertFrom<User>(
        execResult,
        [this](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}

QueryResult<User> MySqlUserDao::getUserByEmail(const std::string& email) {
    QueryResult<ExecuteResult> execResult = execute(
        "SELECT id, discriminator, name, email, is_admin, created_time "
        "FROM users WHERE email = ?",
        email
    );
    
    return QueryResult<User>::convertFrom<User>(
        execResult,
        [this](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}

QueryResult<User> MySqlUserDao::getUserByFullName(const std::string& name, const std::string& discriminator) {
    QueryResult<ExecuteResult> execResult = execute(
        "SELECT id, discriminator, name, email, is_admin, created_time "
        "FROM users WHERE name = ? AND discriminator = ?",
        name, discriminator
    );
    return QueryResult<User>::convertFrom<User>(
        execResult,
        [this](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}