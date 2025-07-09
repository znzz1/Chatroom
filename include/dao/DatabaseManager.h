#pragma once
#include <mysql/mysql.h>
#include <string>
#include <memory>
#include <mutex>

class DatabaseManager {
private:
    static DatabaseManager* instance_;
    static std::mutex mutex_;
    
    MYSQL* mysql_;
    std::string host_;
    int port_;
    std::string username_;
    std::string password_;
    std::string database_;
    bool connected_;
    
    DatabaseManager() : mysql_(nullptr), port_(3306), connected_(false) {}
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

public:
    static DatabaseManager* getInstance();
    static void init(const std::string& host, int port,
                    const std::string& username, const std::string& password,
                    const std::string& database);
    static void cleanup();
    
    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }
    
    bool executeQuery(const std::string& query);
    MYSQL_RES* executeSelect(const std::string& query);
    
    MYSQL* getConnection() { return mysql_; }
    
    ~DatabaseManager();
}; 