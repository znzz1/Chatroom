#include "database/ConnectionPool.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>

ConnectionPool& ConnectionPool::getInstance() {
    static ConnectionPool instance;
    return instance;
}

void ConnectionPool::init(const std::string& host, int port,
                         const std::string& username, const std::string& password,
                         const std::string& database,
                         int min_connections, int max_connections,
                         std::chrono::seconds connection_timeout,
                         std::chrono::seconds idle_timeout) {
    if (initialized_.load()) {
        std::cerr << "ConnectionPool already initialized" << std::endl;
        return;
    }
    
    host_ = host;
    port_ = port;
    username_ = username;
    password_ = password;
    database_ = database;
    min_connections_ = min_connections;
    max_connections_ = max_connections;
    connection_timeout_ = connection_timeout;
    idle_timeout_ = idle_timeout;
    
    std::unique_lock<std::shared_mutex> lock(pool_mutex_);
    
    for (int i = 0; i < min_connections_; ++i) {
        auto conn = createConnection();
        if (conn) {
            idle_connections_.push(conn);
            idle_connection_count_++;
            total_connection_count_++;
        }
    }
    
    initialized_.store(true);
    std::cout << "ConnectionPool initialized with " << min_connections_ << " connections" << std::endl;
}

std::shared_ptr<DatabaseConnection> ConnectionPool::getConnection(std::chrono::milliseconds timeout) {
    if (!initialized_.load()) {
        throw std::runtime_error("ConnectionPool not initialized");
    }
    
    std::unique_lock<std::shared_mutex> lock(pool_mutex_);
    
    auto start_time = std::chrono::steady_clock::now();
    int retry_count = 0;
    const int max_retries = 3;
    
    while (idle_connections_.empty() && active_connection_count_.load() >= max_connections_) {
        if (pool_cv_.wait_until(lock, start_time + timeout) == std::cv_status::timeout) {
            throw std::runtime_error("Timeout waiting for database connection");
        }
        
        if (shutting_down_.load()) {
            throw std::runtime_error("ConnectionPool is shutting down");
        }
        
        // 防止无限等待
        retry_count++;
        if (retry_count > max_retries) {
            throw std::runtime_error("Failed to get connection after maximum retries");
        }
    }
    
    std::shared_ptr<DatabaseConnection> conn;
    
    if (!idle_connections_.empty()) {
        conn = idle_connections_.front();
        idle_connections_.pop();
        idle_connection_count_--;
    } else if (active_connection_count_.load() < max_connections_) {
        // 直接在锁内创建连接，避免竞态
        conn = createConnection();
    }
    
    // 原子性检查和设置连接状态
    if (conn) {
        // 在设置in_use之前再次检查有效性
        if (conn->isValid()) {
            conn->setInUse(true);
            conn->updateLastUsedTime();
            active_connections_.insert(conn);
            active_connection_count_++;
            return conn;
        } else {
            // 无效连接，减少计数
            total_connection_count_--;
        }
    }
    
    // 如果到这里，说明需要创建新连接
    if (active_connection_count_.load() < max_connections_) {
        conn = createConnection();
        
        if (conn && conn->isValid()) {
            conn->setInUse(true);
            conn->updateLastUsedTime();
            active_connections_.insert(conn);
            active_connection_count_++;
            total_connection_count_++;
            return conn;
        }
    }
    
    // 如果所有尝试都失败，抛出异常而不是无限等待
    throw std::runtime_error("Failed to get valid database connection after all attempts");
}

void ConnectionPool::releaseConnection(std::shared_ptr<DatabaseConnection> conn) {
    if (!conn) return;
    
    std::unique_lock<std::shared_mutex> lock(pool_mutex_);
    
    if (shutting_down_.load()) {
        active_connections_.erase(conn);
        active_connection_count_--;
        return;
    }
    
    if (conn->isValid()) {
        conn->reset();
        conn->setInUse(false);
        conn->updateLastUsedTime();
        
        active_connections_.erase(conn);
        active_connection_count_--;
        
        if (idle_connections_.size() >= static_cast<size_t>(min_connections_)) {
            total_connection_count_--;
            return;
        }
        
        idle_connections_.push(conn);
        idle_connection_count_++;
        
        pool_cv_.notify_one();
    } else {
        active_connections_.erase(conn);
        active_connection_count_--;
        total_connection_count_--;
        
        if (total_connection_count_.load() < min_connections_) {
            // 在锁内创建连接，避免竞态条件
            auto new_conn = createConnection();
            if (new_conn) {
                idle_connections_.push(new_conn);
                idle_connection_count_++;
                total_connection_count_++;
                pool_cv_.notify_one();
            }
        }
    }
}

void ConnectionPool::cleanup() {
    if (!initialized_.load()) return;
    
    shutting_down_.store(true);
    
    std::unique_lock<std::shared_mutex> lock(pool_mutex_);
    
    while (!idle_connections_.empty()) {
        idle_connections_.pop();
    }
    
    active_connections_.clear();
    
    idle_connection_count_ = 0;
    total_connection_count_ = 0;
    active_connection_count_ = 0;
    
    initialized_.store(false);
    shutting_down_.store(false);
    
    std::cout << "ConnectionPool cleaned up" << std::endl;
}

int ConnectionPool::getActiveConnections() const {
    std::shared_lock<std::shared_mutex> lock(pool_mutex_);
    return active_connection_count_.load();
}

int ConnectionPool::getIdleConnections() const {
    std::shared_lock<std::shared_mutex> lock(pool_mutex_);
    return idle_connection_count_.load();
}

int ConnectionPool::getTotalConnections() const {
    std::shared_lock<std::shared_mutex> lock(pool_mutex_);
    return total_connection_count_.load();
}

void ConnectionPool::healthCheck() {
    auto now = std::chrono::steady_clock::now();
    auto last_check = last_health_check_.load();
    
    if (now - last_check < health_check_interval_) {
        return;
    }
    
    last_health_check_.store(now);
    
    std::unique_lock<std::shared_mutex> lock(pool_mutex_);
    
    removeInvalidConnections();
    
    cleanupExpiredConnections();
    
    while (idle_connections_.size() < static_cast<size_t>(min_connections_) && 
           total_connection_count_.load() < max_connections_) {
        auto conn = createConnection();
        if (conn) {
            idle_connections_.push(conn);
            idle_connection_count_++;
            total_connection_count_++;
        } else {
            break;
        }
    }
}

void ConnectionPool::removeInvalidConnections() {
    std::queue<std::shared_ptr<DatabaseConnection>> valid_connections;
    int valid_count = 0;
    
    while (!idle_connections_.empty()) {
        auto conn = idle_connections_.front();
        idle_connections_.pop();
        
        if (conn->isValid()) {
            valid_connections.push(conn);
            valid_count++;
        } else {
            total_connection_count_--;
        }
    }
    
    idle_connections_ = std::move(valid_connections);
    idle_connection_count_ = valid_count;
}

void ConnectionPool::cleanupExpiredConnections() {
    auto now = std::chrono::steady_clock::now();
    
    std::queue<std::shared_ptr<DatabaseConnection>> valid_connections;
    int valid_count = 0;
    
    while (!idle_connections_.empty()) {
        auto conn = idle_connections_.front();
        idle_connections_.pop();
        
        auto last_used = conn->getLastUsedTime();
        if (now - last_used < idle_timeout_) {
            valid_connections.push(conn);
            valid_count++;
        } else {
            total_connection_count_--;
        }
    }

    idle_connections_ = std::move(valid_connections);
    idle_connection_count_ = valid_count;
}

std::shared_ptr<DatabaseConnection> ConnectionPool::createConnection() {
    MYSQL* mysql = mysql_init(nullptr);
    if (!mysql) {
        std::cerr << "Failed to initialize MySQL connection" << std::endl;
        return nullptr;
    }
    
    int timeout = 10;
    mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(mysql, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    
    bool reconnect = true;
    mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);
    
    if (!mysql_real_connect(mysql, host_.c_str(), username_.c_str(), 
                           password_.c_str(), database_.c_str(), port_, nullptr, 0)) {
        std::cerr << "Failed to connect to MySQL: " << mysql_error(mysql) << std::endl;
        mysql_close(mysql);
        return nullptr;
    }
    
    mysql_set_character_set(mysql, "utf8mb4");
    
    return std::make_shared<DatabaseConnection>(mysql);
} 