#pragma once
#include <string>
#include <vector>
#include "models/Message.h"
#include "models/Enums.h"
#include "utils/QueryResult.h"

class MessageDao {
public:
    virtual ~MessageDao() = default;
    
    virtual MessageOperationResult sendMessageToRoom(int userId, int roomId, const std::string& content, const std::string& displayName) = 0;
    virtual QueryResult<std::vector<Message>> getRecentMessages(int roomId, int max_count = 50) = 0;
    virtual QueryResult<std::vector<Message>> getRecentMessagesByUser(int userId, int roomId, int max_count = 50) = 0;
}; 