#include "database/PreparedStatement.h"
#include <cstring>
#include <stdexcept>

PreparedStatement::PreparedStatement(std::shared_ptr<DatabaseConnection> conn, const std::string& sql, int param_count)
    : connection_(conn), current_param_idx_(0) {
    stmt_ = mysql_stmt_init(connection_->getConnection());
    if (!stmt_) throw std::runtime_error("mysql_stmt_init failed");
    
    if (mysql_stmt_prepare(stmt_, sql.c_str(), sql.length()) != 0)
        throw std::runtime_error(mysql_stmt_error(stmt_));

    // 验证参数数量是否匹配
    int actual_param_count = mysql_stmt_param_count(stmt_);
    if (actual_param_count != param_count) {
        mysql_stmt_close(stmt_);
        throw std::runtime_error("Parameter count mismatch: expected " + 
                                std::to_string(param_count) + ", got " + 
                                std::to_string(actual_param_count));
    }

    param_binds_.resize(param_count);
    memset(param_binds_.data(), 0, sizeof(MYSQL_BIND) * param_count);
    
    param_values_.resize(param_count);
}

PreparedStatement::~PreparedStatement() {
    if (stmt_) {
        mysql_stmt_close(stmt_);
    }
}

PreparedStatement& PreparedStatement::bind(int value) {
    if (current_param_idx_ < param_binds_.size()) {
        param_values_[current_param_idx_] = value;
        param_binds_[current_param_idx_].buffer_type = MYSQL_TYPE_LONG;
        param_binds_[current_param_idx_].buffer = &std::get<int>(param_values_[current_param_idx_]);
        current_param_idx_++;
    }
    return *this;
}

PreparedStatement& PreparedStatement::bind(const std::string& value) {
    if (current_param_idx_ < param_binds_.size()) {
        // 直接存储字符串，保持类型一致性
        param_values_[current_param_idx_] = value;
        
        // 使用更安全的方法，避免指针失效
        auto& stored_string = std::get<std::string>(param_values_[current_param_idx_]);
        
        param_binds_[current_param_idx_].buffer_type = MYSQL_TYPE_STRING;
        param_binds_[current_param_idx_].buffer = const_cast<char*>(stored_string.c_str());
        param_binds_[current_param_idx_].buffer_length = stored_string.length();
        current_param_idx_++;
    }
    return *this;
}

PreparedStatement& PreparedStatement::bind(double value) {
    if (current_param_idx_ < param_binds_.size()) {
        param_values_[current_param_idx_] = value;
        param_binds_[current_param_idx_].buffer_type = MYSQL_TYPE_DOUBLE;
        param_binds_[current_param_idx_].buffer = &std::get<double>(param_values_[current_param_idx_]);
        current_param_idx_++;
    }
    return *this;
}

PreparedStatement& PreparedStatement::bind(bool value) {
    if (current_param_idx_ < param_binds_.size()) {
        int int_value = value ? 1 : 0;
        param_values_[current_param_idx_] = int_value;
        param_binds_[current_param_idx_].buffer_type = MYSQL_TYPE_TINY;
        param_binds_[current_param_idx_].buffer = &std::get<int>(param_values_[current_param_idx_]);
        current_param_idx_++;
    }
    return *this;
}

bool PreparedStatement::execute() {
    if (!param_binds_.empty()) {
        if (mysql_stmt_bind_param(stmt_, param_binds_.data()) != 0) {
            return false;
        }
    }
    
    bool result = mysql_stmt_execute(stmt_) == 0;
    
    // 重置参数索引，允许重用
    current_param_idx_ = 0;
    
    return result;
}

std::string PreparedStatement::getLastError() const {
    if (stmt_) {
        return mysql_stmt_error(stmt_);
    }
    return "No statement available";
}

 