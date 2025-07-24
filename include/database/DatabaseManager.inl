#pragma once

#include "database/DatabaseManager.h"
#include "database/ConnectionPool.h"
#include "database/PreparedStatement.h"
#include <algorithm>

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
            return QueryResult<ExecuteResult>::InternalError(error_msg);
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
                    return QueryResult<ExecuteResult>::InternalError("Failed to fetch rows");
                }
            }
            
            return QueryResult<ExecuteResult>::Success(results);
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