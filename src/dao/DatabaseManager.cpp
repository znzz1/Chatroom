#include "dao/DatabaseManager.h"
#include <iostream>

DatabaseManager* DatabaseManager::instance_ = nullptr;
std::mutex DatabaseManager::mutex_;

DatabaseManager* DatabaseManager::getInstance() {
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instance_ == nullptr) {
            instance_ = new DatabaseManager();
        }
    }
    return instance_;
}

void DatabaseManager::init(const std::string& host, int port,
                          const std::string& username, const std::string& password,
                          const std::string& database) {
    DatabaseManager* manager = getInstance();
    manager->host_ = host;
    manager->port_ = port;
    manager->username_ = username;
    manager->password_ = password;
    manager->database_ = database;
    manager->connect();
}

void DatabaseManager::cleanup() {
    if (instance_) {
        instance_->disconnect();
        delete instance_;
        instance_ = nullptr;
    }
}

bool DatabaseManager::connect() {
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        std::cerr << "Failed to initialize MySQL" << std::endl;
        return false;
    }
    
    if (!mysql_real_connect(mysql_, host_.c_str(), username_.c_str(), 
                           password_.c_str(), database_.c_str(), port_, nullptr, 0)) {
        std::cerr << "Failed to connect to MySQL: " << mysql_error(mysql_) << std::endl;
        return false;
    }
    
    mysql_set_character_set(mysql_, "utf8mb4");
    connected_ = true;
    return true;
}

void DatabaseManager::disconnect() {
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
        connected_ = false;
    }
}

bool DatabaseManager::executeQuery(const std::string& query) {
    if (!connected_ || !mysql_) return false;
    
    if (mysql_query(mysql_, query.c_str()) != 0) {
        std::cerr << "Query failed: " << mysql_error(mysql_) << std::endl;
        return false;
    }
    return true;
}

MYSQL_RES* DatabaseManager::executeSelect(const std::string& query) {
    if (!connected_ || !mysql_) return nullptr;
    
    if (mysql_query(mysql_, query.c_str()) != 0) {
        std::cerr << "Select query failed: " << mysql_error(mysql_) << std::endl;
        return nullptr;
    }
    return mysql_store_result(mysql_);
}

DatabaseManager::~DatabaseManager() {
    disconnect();
} 