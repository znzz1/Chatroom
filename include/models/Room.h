#pragma once
#include "models/Enums.h"
#include <string>
#include <chrono>
#include <ctime>

struct Room {
    int id = 0;
    std::string name;
    std::string description;
    int creator_id = 0;
    int max_users = 0;
    int current_users = 0;
    RoomStatus status = RoomStatus::ACTIVE;
    std::string created_time;
    
    Room() = default;
    Room(const std::string& room_name, const std::string& desc, int creator, int max = 0)
        : id(0), name(room_name), description(desc), creator_id(creator), 
          max_users(max), current_users(0), status(RoomStatus::ACTIVE) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        created_time = std::ctime(&time_t);
        if (!created_time.empty() && created_time.back() == '\n') {
            created_time.pop_back();
        }
    }
};