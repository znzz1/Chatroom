#pragma once
#include "database/DatabaseManager.h"
#include "utils/QueryResult.h"
#include "database/ExecuteResult.h"
#include <string>
#include <memory>
#include <vector>

template<typename T>
class SqlDao {
public:
    virtual ~SqlDao() = default;
    
    template<typename... Args>
    QueryResult<ExecuteResult> execute(const std::string& sql, Args... args) {
        return DatabaseManager::getInstance().execute(sql, args...);
    }
    
    template<typename... Args>
    QueryResult<ExecuteResult> execute(std::shared_ptr<DatabaseConnection> conn, const std::string& sql, Args... args) {
        return DatabaseManager::getInstance().execute(conn, sql, args...);
    }
    
    template<typename Func>
    QueryResult<ExecuteResult> executeTransaction(Func&& func) {
        return DatabaseManager::getInstance().executeTransaction(std::forward<Func>(func));
    }
    
protected:
    virtual T createFromResultSet(const std::vector<std::string>& row) = 0;
};

 