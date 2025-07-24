#pragma once
#include <string>

struct Room {
    int id = 0;
    std::string name;
    std::string description;
    int creator_id = 0;
    int max_users = 0;
    bool is_active = true;
    std::string created_time;
    
    Room() = default;
    Room(int id, const std::string& name, const std::string& description, int creator_id, int max_users, bool is_active, const std::string& created_time)
        : id(id), name(name), description(description), creator_id(creator_id), max_users(max_users), is_active(is_active), created_time(created_time) {}
};