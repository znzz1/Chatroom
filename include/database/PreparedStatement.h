#pragma once
#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include "database/ConnectionPool.h"

class PreparedStatement {
public:
    PreparedStatement(std::shared_ptr<DatabaseConnection> conn, const std::string& sql, int param_count);
    ~PreparedStatement();

    PreparedStatement& bind(int value);
    PreparedStatement& bind(const std::string& value);
    PreparedStatement& bind(double value);
    PreparedStatement& bind(bool value);

    bool execute();
    std::string getLastError() const;
    MYSQL_STMT* getStmt() const { return stmt_; }

private:
    std::shared_ptr<DatabaseConnection> connection_;
    MYSQL_STMT* stmt_;
    std::vector<MYSQL_BIND> param_binds_;
    std::vector<std::variant<int, double, std::string>> param_values_;
    size_t current_param_idx_;
};