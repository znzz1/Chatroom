#pragma once
#include <string>

struct User {
    int id = 0;
    std::string discriminator;
    std::string name;
    std::string email;
    bool is_admin = false;
    std::string created_time;
    
    User() = default;
    
    User(int id, const std::string& discriminator, const std::string& name, const std::string& email, bool is_admin, const std::string& created_time)
        : id(id), discriminator(discriminator), name(name), email(email), is_admin(is_admin), created_time(created_time) {}
}; 