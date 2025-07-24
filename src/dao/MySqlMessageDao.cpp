#include "dao/MySqlMessageDao.h"
#include <iostream>

Message MySqlMessageDao::createFromResultSet(const std::vector<std::string>& row) {
    return Message{std::stoi(row[0]), std::stoi(row[1]), std::stoi(row[2]), row[3], row[4], row[5]};
}

QueryResult<void> MySqlMessageDao::sendMessageToRoom(
    int userId,
    int roomId,
    const std::string& content,
    const std::string& displayName,
    const std::string& sendTime
) {
    QueryResult<ExecuteResult> result = execute(
        "INSERT INTO messages (user_id, room_id, content, display_name, send_time) VALUES (?, ?, ?, ?, ?)",
        userId, roomId, content, displayName, sendTime
    );

    return QueryResult<void>::convertFrom(result);
}

QueryResult<std::vector<Message>> MySqlMessageDao::getRecentMessages(int roomId, int max_count) {
    QueryResult<ExecuteResult> result = execute(
        "SELECT message_id, user_id, room_id, content, display_name, send_time "
        "FROM messages WHERE room_id = ? ORDER BY send_time DESC LIMIT ?",
        roomId, max_count
    );

    return QueryResult<std::vector<Message>>::convertFromMultiple<std::vector<Message>>(
        result,
        [this](const std::vector<std::vector<std::string>>& rows) {
            std::vector<Message> messages;
            for (const auto& row : rows) {
                messages.push_back(createFromResultSet(row));
            }
            return messages;
        }
    );
}

QueryResult<std::vector<Message>> MySqlMessageDao::getRecentMessagesByUser(int userId, int roomId, int max_count) {
    QueryResult<ExecuteResult> result = execute(
        "SELECT message_id, user_id, room_id, content, display_name, send_time "
        "FROM messages WHERE user_id = ? AND room_id = ? ORDER BY send_time DESC LIMIT ?",
        userId, roomId, max_count
    );
    
    return QueryResult<std::vector<Message>>::convertFromMultiple<std::vector<Message>>(
        result,
        [this](const std::vector<std::vector<std::string>>& rows) {
            std::vector<Message> messages;
            for (const auto& row : rows) {
                messages.push_back(createFromResultSet(row));
            }
            return messages;
        }
    );
}
