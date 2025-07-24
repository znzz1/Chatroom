#pragma once
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

class TimeUtils {
public:
    // 获取当前时间的标准格式字符串 (YYYY-MM-DD HH:MM:SS)
    static std::string getCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
    
    // 获取当前时间的ctime格式字符串 (Day Mon DD HH:MM:SS YYYY)
    static std::string getCurrentCtimeString() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::string time_str = std::ctime(&time_t);
        if (!time_str.empty() && time_str.back() == '\n') {
            time_str.pop_back();
        }
        return time_str;
    }
    
    // 将MySQL DATETIME格式转换为ctime格式
    static std::string mysqlToCtime(const std::string& mysql_time) {
        if (mysql_time.empty()) {
            return getCurrentCtimeString();
        }
        
        std::tm tm = {};
        std::istringstream ss(mysql_time);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        
        if (ss.fail()) {
            return getCurrentCtimeString();
        }
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%a %b %d %H:%M:%S %Y");
        return oss.str();
    }
    
    // 将ctime格式转换为MySQL DATETIME格式
    static std::string ctimeToMysql(const std::string& ctime_str) {
        if (ctime_str.empty()) {
            return getCurrentTimeString();
        }
        
        std::tm tm = {};
        std::istringstream ss(ctime_str);
        ss >> std::get_time(&tm, "%a %b %d %H:%M:%S %Y");
        
        if (ss.fail()) {
            return getCurrentTimeString();
        }
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
    
    // 获取当前时间戳（毫秒）
    static int64_t getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }
}; 