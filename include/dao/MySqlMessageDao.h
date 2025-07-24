#pragma once
#include "dao/MessageDao.h"
#include "dao/SqlDao.h"
#include <memory>

class MySqlMessageDao : public MessageDao, public SqlDao<Message> {
public:
    ~MySqlMessageDao() override = default;

    QueryResult<void> sendMessageToRoom(int userId, int roomId, const std::string& content, const std::string& displayName, const std::string& sendTime) override;
    QueryResult<std::vector<Message>> getRecentMessages(int roomId, int max_count = 50) override;
    QueryResult<std::vector<Message>> getRecentMessagesByUser(int userId, int roomId, int max_count = 50) override;
protected:
    Message createFromResultSet(const std::vector<std::string>& row) override;
};
