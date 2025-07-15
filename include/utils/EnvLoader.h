#pragma once
#include <string>
#include <optional>
#include <cstdlib>

class EnvLoader {
public:
    // 获取字符串环境变量
    static std::optional<std::string> getString(const std::string& name);
    
    // 获取整数环境变量
    static std::optional<int> getInt(const std::string& name);
    
    // 获取布尔环境变量
    static std::optional<bool> getBool(const std::string& name);
    
    // 检查环境变量是否存在
    static bool exists(const std::string& name);
    
    // 设置环境变量
    static bool set(const std::string& name, const std::string& value);
    
    // 从文件加载环境变量（.env格式）
    static bool loadFromFile(const std::string& filename);
}; 