#pragma once
#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <unordered_set>

class DatabaseConnection {
public:
    explicit DatabaseConnection(MYSQL* conn) : connection_(conn), in_use_(false) {}
    ~DatabaseConnection() {
        if (connection_) {
            mysql_close(connection_);
        }
    }
    
    MYSQL* getConnection() const { return connection_; }
    bool isInUse() const { return in_use_.load(); }
    void setInUse(bool in_use) { in_use_.store(in_use); }
    
    bool isValid() const {
        if (!connection_) return false;
        return mysql_ping(connection_) == 0;
    }
    
    void reset() {
        if (connection_) {
            mysql_reset_connection(connection_);
        }
    }
    
    bool beginTransaction() {
        if (!connection_) return false;
        return mysql_query(connection_, "START TRANSACTION") == 0;
    }
    
    bool commit() {
        if (!connection_) return false;
        return mysql_query(connection_, "COMMIT") == 0;
    }
    
    bool rollback() {
        if (!connection_) return false;
        return mysql_query(connection_, "ROLLBACK") == 0;
    }
    
    std::chrono::steady_clock::time_point getCreatedTime() const { return created_time_; }
    
    std::chrono::steady_clock::time_point getLastUsedTime() const { return last_used_time_.load(); }
    void updateLastUsedTime() { last_used_time_.store(std::chrono::steady_clock::now()); }

private:
    MYSQL* connection_;
    std::atomic<bool> in_use_{false};
    std::chrono::steady_clock::time_point created_time_{std::chrono::steady_clock::now()};
    std::atomic<std::chrono::steady_clock::time_point> last_used_time_{std::chrono::steady_clock::now()};
};

class ConnectionPool {
public:
    static ConnectionPool& getInstance();
    
    void init(const std::string& host, int port,
              const std::string& username, const std::string& password,
              const std::string& database,
              int min_connections, int max_connections,
              std::chrono::seconds connection_timeout,
              std::chrono::seconds idle_timeout);
    
    std::shared_ptr<DatabaseConnection> getConnection(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    void releaseConnection(std::shared_ptr<DatabaseConnection> conn);
    
    void cleanup();
    
    int getActiveConnections() const;
    int getIdleConnections() const;
    int getTotalConnections() const;
    
    void healthCheck();

private:
    ConnectionPool() = default;
    ~ConnectionPool() = default;
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    
    std::shared_ptr<DatabaseConnection> createConnection();
    void removeInvalidConnections();
    void cleanupExpiredConnections();
    
    std::string host_;
    int port_;
    std::string username_;
    std::string password_;
    std::string database_;
    
    int min_connections_;
    int max_connections_;
    std::chrono::seconds connection_timeout_;
    std::chrono::seconds idle_timeout_;
    
    std::queue<std::shared_ptr<DatabaseConnection>> idle_connections_;
    std::unordered_set<std::shared_ptr<DatabaseConnection>> active_connections_;
    mutable std::shared_mutex pool_mutex_;
    std::condition_variable_any pool_cv_;
    
    std::atomic<int> active_connection_count_{0};
    std::atomic<int> idle_connection_count_{0};
    std::atomic<int> total_connection_count_{0};
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};
    
    std::atomic<std::chrono::steady_clock::time_point> last_health_check_{std::chrono::steady_clock::now()};
    std::chrono::seconds health_check_interval_{std::chrono::seconds(60)};
}; 