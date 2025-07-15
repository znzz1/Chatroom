#include "dao/MySqlMessageDao.h"
#include <iostream>

Message MySqlMessageDao::createFromResultSet(const std::vector<std::string>& row) {
    Message message;
    if (row.size() >= 6) {
        message.message_id = std::stoi(row[0]);
        message.user_id = std::stoi(row[1]);
        message.room_id = std::stoi(row[2]);
        message.content = row[3];
        message.display_name = row[4];
        message.timestamp = row[5];
    }
    return message;
}

MessageOperationResult MySqlMessageDao::sendMessageToRoom(int userId, int roomId, const std::string& content, const std::string& displayName) {
    auto result = execute(
        "INSERT INTO messages (user_id, room_id, content, display_name, send_time) VALUES (?, ?, ?, ?, NOW())",
        userId, roomId, content, displayName
    );

    if (result.isConnectionError()) return MessageOperationResult::SQL_CONNECTION_ERROR;
    if (result.isInternalError()) return MessageOperationResult::DATABASE_INTERNAL_ERROR;

    return MessageOperationResult::SUCCESS;
}

QueryResult<std::vector<Message>> MySqlMessageDao::getRecentMessages(int roomId, int max_count) {
    auto result = execute(
        "SELECT message_id, user_id, room_id, content, display_name, send_time "
        "FROM messages WHERE room_id = ? ORDER BY send_time DESC LIMIT ?",
        roomId, max_count
    );

    return QueryResult<std::vector<Message>>::convertFrom(
        result,
        [](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}

QueryResult<std::vector<Message>> MySqlMessageDao::getRecentMessagesByUser(int userId, int roomId, int max_count) {
    auto result = execute(
        "SELECT message_id, user_id, room_id, content, display_name, send_time "
        "FROM messages WHERE user_id = ? AND room_id = ? ORDER BY send_time DESC LIMIT ?",
        userId, roomId, max_count
    );
    
    return QueryResult<std::vector<Message>>::convertFrom(
        result,
        [](const std::vector<std::string>& row) {
            return createFromResultSet(row);
        }
    );
}
