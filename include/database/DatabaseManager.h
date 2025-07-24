#pragma once
#include <string>
#include <memory>
#include <vector>
#include <cstring>
#include <mysql/mysql.h>
#include "utils/QueryResult.h"
#include "database/ExecuteResult.h"

class DatabaseConnection;

class DatabaseManager {
private:
    std::string host_;
    int port_;
    std::string username_;
    std::string password_;
    std::string database_;
    bool initialized_;
    
    DatabaseManager() : port_(3306), initialized_(false) {}
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
public:
    static DatabaseManager& getInstance();
    
    static void init();
    static void cleanup();
    
    bool isInitialized() const { return initialized_; }
    
    template<typename... Args>
    QueryResult<ExecuteResult> execute(const std::string& sql, Args... args);
    
    template<typename... Args>
    QueryResult<ExecuteResult> execute(std::shared_ptr<DatabaseConnection> conn, const std::string& sql, Args... args);
    
    template<typename Func>
    QueryResult<ExecuteResult> executeTransaction(Func&& func);
    
    ~DatabaseManager() = default;
};

#include "database/DatabaseManager.inl" 