#include "database/DatabaseManager.h"
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

template QueryResult<ExecuteResult> DatabaseManager::execute(const std::string&, const std::string&);
template QueryResult<ExecuteResult> DatabaseManager::execute(const std::string&, int);
template QueryResult<ExecuteResult> DatabaseManager::execute(const std::string&, const std::string&, int);
template QueryResult<ExecuteResult> DatabaseManager::execute(std::shared_ptr<DatabaseConnection>, const std::string&, const std::string&);
template QueryResult<ExecuteResult> DatabaseManager::execute(std::shared_ptr<DatabaseConnection>, const std::string&, int);
template QueryResult<ExecuteResult> DatabaseManager::execute(std::shared_ptr<DatabaseConnection>, const std::string&, const std::string&, int);