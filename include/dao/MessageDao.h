#pragma once
#include <string>
#include <vector>
#include "models/Message.h"
#include "utils/QueryResult.h"

class MessageDao {
public:
    virtual ~MessageDao() = default;
    
    virtual QueryResult<void> sendMessageToRoom(int userId, int roomId, const std::string& content, const std::string& displayName, const std::string& sendTime) = 0;
    virtual QueryResult<std::vector<Message>> getRecentMessages(int roomId, int max_count = 50) = 0;
    virtual QueryResult<std::vector<Message>> getRecentMessagesByUser(int userId, int roomId, int max_count = 50) = 0;
}; 