#pragma once
#include "models/Enums.h"
#include <string>
#include <chrono>
#include <ctime>

struct User {
    int id = 0;
    std::string discriminator;
    std::string name;
    std::string email;
    UserRole role = UserRole::NORMAL;
    std::string created_time;
    
    User() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        created_time = std::ctime(&time_t);
        if (!created_time.empty() && created_time.back() == '\n') {
            created_time.pop_back();
        }
    }
    
    User(const std::string& name, const std::string& email, UserRole r = UserRole::NORMAL) 
        : id(0), discriminator(""), name(name), email(email), role(r) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        created_time = std::ctime(&time_t);
        if (!created_time.empty() && created_time.back() == '\n') {
            created_time.pop_back();
        }
    }
    
    std::string getFullName() const {
        return name + "#" + discriminator;
    }
}; 