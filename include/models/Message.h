#pragma once
#include <string>

struct Message {
    int message_id = 0;
    int user_id = 0;
    int room_id = 0;
    std::string content;
    std::string display_name;
    std::string timestamp;
    
    Message() = default;
    
    Message(int mid, int uid, int rid, const std::string& cont, 
            const std::string& name, const std::string& time)
        : message_id(mid), user_id(uid), room_id(rid), content(cont), 
          display_name(name), timestamp(time) {}
}; 