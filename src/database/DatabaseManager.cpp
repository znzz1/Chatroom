#include "database/DatabaseManager.h"
#include "database/PreparedStatement.h"
#include "utils/EnvLoader.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <chrono>
#include <algorithm>

QueryResult<ExecuteResult> createErrorResult(const std::string& error_msg) {
    std::string lower_error = error_msg;
    std::transform(lower_error.begin(), lower_error.end(), lower_error.begin(), ::tolower);
    
    if (lower_error.find("connection") != std::string::npos ||
        lower_error.find("timeout") != std::string::npos ||
        lower_error.find("refused") != std::string::npos ||
        lower_error.find("lost") != std::string::npos ||
        lower_error.find("network") != std::string::npos) {
        return QueryResult<ExecuteResult>::ConnectionError(error_msg);
    }
    
    return QueryResult<ExecuteResult>::InternalError(error_msg);
}

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

void DatabaseManager::init() {
    const std::vector<std::string> config_files = {
        ".env",
        "config/database.env",
        "config/.env"
    };
    
    for (const auto& file : config_files) {
        if (EnvLoader::loadFromFile(file)) {
            break;
        }
    }
    
    const char* host = std::getenv("DB_HOST");
    const char* port = std::getenv("DB_PORT");
    const char* username = std::getenv("DB_USERNAME");
    const char* password = std::getenv("DB_PASSWORD");
    const char* database = std::getenv("DB_DATABASE");
    
    if (!host || !username || !password || !database) {
        throw std::runtime_error("Missing required database environment variables");
    }
    
    int port_num = port ? std::stoi(port) : 3306;
    
    DatabaseManager& manager = getInstance();
    manager.host_ = host;
    manager.port_ = port_num;
    manager.username_ = username;
    manager.password_ = password;
    manager.database_ = database;
    
    ConnectionPool::getInstance().init(host, port_num, username, password, database, 
                                      5, 20, std::chrono::seconds(300), std::chrono::seconds(600));
    manager.initialized_ = true;
}

void DatabaseManager::cleanup() {
    ConnectionPool::getInstance().cleanup();
    DatabaseManager& manager = getInstance();
    manager.initialized_ = false;
}

template<typename... Args>
QueryResult<ExecuteResult> DatabaseManager::execute(const std::string& sql, Args... args) {
    if (!initialized_) {
        return QueryResult<ExecuteResult>::InternalError("Database not initialized");
    }
    
    auto conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        return QueryResult<ExecuteResult>::ConnectionError("Failed to get database connection");
    }
    
    auto result = execute(conn, sql, args...);
    ConnectionPool::getInstance().releaseConnection(conn);
    return result;
}

template<typename... Args>
QueryResult<ExecuteResult> DatabaseManager::execute(std::shared_ptr<DatabaseConnection> conn, const std::string& sql, Args... args) {
    if (!initialized_ || !conn) {
        return QueryResult<ExecuteResult>::InternalError("Database not initialized or invalid connection");
    }
    
    try {
        auto stmt = std::make_unique<PreparedStatement>(conn, sql, sizeof...(args));
        if (!stmt) {
            return QueryResult<ExecuteResult>::InternalError("Failed to create PreparedStatement");
        }
        
        (stmt->bind(args), ...);
        
        if (!stmt->execute()) {
            std::string error_msg = stmt->getLastError();
            if (error_msg.empty()) {
                error_msg = "Failed to execute SQL statement";
            }
            return createErrorResult(error_msg);
        }
        
        MYSQL_RES* meta_result = mysql_stmt_result_metadata(stmt->getStmt());
        if (!meta_result) {
            return QueryResult<ExecuteResult>::Success(std::monostate{});
        }
        
        int column_count = mysql_num_fields(meta_result);
        if (column_count == 0) {
            mysql_free_result(meta_result);
            return QueryResult<ExecuteResult>::Success(std::monostate{});
        }
        
        if (mysql_stmt_store_result(stmt->getStmt()) != 0) {
            mysql_free_result(meta_result);
            return QueryResult<ExecuteResult>::InternalError("Failed to store result");
        }
        
        my_ulonglong row_count = mysql_stmt_num_rows(stmt->getStmt());
        
        std::vector<MYSQL_BIND> result_binds(column_count);
        std::vector<std::string> string_buffers(column_count);
        std::vector<unsigned long> lengths(column_count);
        
        memset(result_binds.data(), 0, sizeof(MYSQL_BIND) * column_count);
        
        const size_t initial_buffer_size = 1024;
        
        for (int i = 0; i < column_count; ++i) {
            string_buffers[i].resize(initial_buffer_size);
            result_binds[i].buffer_type = MYSQL_TYPE_STRING;
            result_binds[i].buffer = string_buffers[i].data();
            result_binds[i].buffer_length = initial_buffer_size;
            result_binds[i].length = &lengths[i];
        }
        
        mysql_free_result(meta_result);
        
        if (mysql_stmt_bind_result(stmt->getStmt(), result_binds.data()) != 0) {
            return QueryResult<ExecuteResult>::InternalError("Failed to bind result");
        }
        
        if (row_count == 0) {
            return QueryResult<ExecuteResult>::NotFound();
        } else if (row_count == 1) {
            int fetch_result = mysql_stmt_fetch(stmt->getStmt());
            if (fetch_result != 0) {
                return QueryResult<ExecuteResult>::InternalError("Failed to fetch single row");
            }
            
            std::vector<std::string> row;
            row.reserve(column_count);
            for (int i = 0; i < column_count; ++i) {
                row.emplace_back(string_buffers[i].data(), lengths[i]);
            }
            
            return QueryResult<ExecuteResult>::Success(row);
        } else {
            std::vector<std::vector<std::string>> results;
            
            while (true) {
                int fetch_result = mysql_stmt_fetch(stmt->getStmt());
                if (fetch_result == 0) {
                    std::vector<std::string> row;
                    row.reserve(column_count);
                    for (int i = 0; i < column_count; ++i) {
                        row.emplace_back(string_buffers[i].data(), lengths[i]);
                    }
                    results.push_back(std::move(row));
                } else if (fetch_result == MYSQL_NO_DATA) {
                    break;
                } else {
                    return QueryResult<ExecuteResult>::InternalError("Failed to fetch row");
                }
            }
            
            return QueryResult<ExecuteResult>::Success(std::move(results));
        }
        
    } catch (const std::exception& e) {
        return QueryResult<ExecuteResult>::InternalError(std::string("Exception: ") + e.what());
    }
}

template<typename Func>
QueryResult<ExecuteResult> DatabaseManager::executeTransaction(Func&& func) {
    if (!initialized_) {
        return QueryResult<ExecuteResult>::InternalError("Database not initialized");
    }
    
    auto conn = ConnectionPool::getInstance().getConnection();
    if (!conn) {
        return QueryResult<ExecuteResult>::ConnectionError("Failed to get database connection");
    }
    
    try {
        if (!conn->beginTransaction()) {
            ConnectionPool::getInstance().releaseConnection(conn);
            return QueryResult<ExecuteResult>::InternalError("Failed to begin transaction");
        }
        
        auto result = func(conn);
        
        if (result.isSuccess()) {
            if (!conn->commit()) {
                conn->rollback();
                ConnectionPool::getInstance().releaseConnection(conn);
                return QueryResult<ExecuteResult>::InternalError("Failed to commit transaction");
            }
            ConnectionPool::getInstance().releaseConnection(conn);
            return result;
        } else {
            conn->rollback();
            ConnectionPool::getInstance().releaseConnection(conn);
            return result;
        }
        
    } catch (const std::exception& e) {
        conn->rollback();
        ConnectionPool::getInstance().releaseConnection(conn);
        return QueryResult<ExecuteResult>::InternalError(std::string("Exception: ") + e.what());
    }
}

template QueryResult<ExecuteResult> DatabaseManager::execute(const std::string&, const std::string&);
template QueryResult<ExecuteResult> DatabaseManager::execute(const std::string&, int);
template QueryResult<ExecuteResult> DatabaseManager::execute(const std::string&, const std::string&, int);
template QueryResult<ExecuteResult> DatabaseManager::execute(std::shared_ptr<DatabaseConnection>, const std::string&, const std::string&);
template QueryResult<ExecuteResult> DatabaseManager::execute(std::shared_ptr<DatabaseConnection>, const std::string&, int);
template QueryResult<ExecuteResult> DatabaseManager::execute(std::shared_ptr<DatabaseConnection>, const std::string&, const std::string&, int);