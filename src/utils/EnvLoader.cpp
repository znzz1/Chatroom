#include "utils/EnvLoader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

std::optional<std::string> EnvLoader::getString(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value) {
        return std::string(value);
    }
    return std::nullopt;
}

std::optional<int> EnvLoader::getInt(const std::string& name) {
    auto str_value = getString(name);
    if (!str_value) return std::nullopt;
    
    try {
        return std::stoi(*str_value);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse environment variable " << name 
                  << " as integer: " << *str_value << std::endl;
        return std::nullopt;
    }
}

std::optional<bool> EnvLoader::getBool(const std::string& name) {
    auto str_value = getString(name);
    if (!str_value) return std::nullopt;
    
    std::string value = *str_value;
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    } else if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    
    std::cerr << "Failed to parse environment variable " << name 
              << " as boolean: " << *str_value << std::endl;
    return std::nullopt;
}

bool EnvLoader::exists(const std::string& name) {
    return std::getenv(name.c_str()) != nullptr;
}

bool EnvLoader::set(const std::string& name, const std::string& value) {
#ifdef _WIN32
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
    return setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
}

bool EnvLoader::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open environment file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    int line_number = 0;
    bool success = true;
    
    while (std::getline(file, line)) {
        line_number++;
        
        // 跳过空行和注释行
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // 查找等号分隔符
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            std::cerr << "Invalid environment variable format at line " 
                      << line_number << ": " << line << std::endl;
            success = false;
            continue;
        }
        
        // 提取键和值
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // 去除首尾空格
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // 去除值的引号
        if (value.length() >= 2 && 
            ((value[0] == '"' && value[value.length()-1] == '"') ||
             (value[0] == '\'' && value[value.length()-1] == '\''))) {
            value = value.substr(1, value.length() - 2);
        }
        
        // 设置环境变量
        if (!set(key, value)) {
            std::cerr << "Failed to set environment variable " << key 
                      << " at line " << line_number << std::endl;
            success = false;
        }
    }
    
    file.close();
    return success;
} 