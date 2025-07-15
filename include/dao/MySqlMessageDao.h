#pragma once
#include "dao/MessageDao.h"
#include <vector>
#include <string>

class MySqlMessageDao : public MessageDao {
public:
    ~MySqlMessageDao() override = default;

    MessageOperationResult sendMessageToRoom(int userId, int roomId, const std::string& content, const std::string& displayName) override;
    QueryResult<std::vector<Message>> getRecentMessages(int roomId, int max_count = 50) override;
    QueryResult<std::vector<Message>> getRecentMessagesByUser(int userId, int roomId, int max_count = 50) override;
protected:
    Message createFromResultSet(const std::vector<std::string>& row) override;
};
