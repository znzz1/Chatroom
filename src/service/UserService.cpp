#include "service/UserService.h"
#include "utils/PasswordHasher.h"

UserService::UserService() {
}

ServiceResult<User> UserService::registerUser(const std::string& email, const std::string& password, const std::string& name) {
    auto userDao = getUserDao();
    if (!userDao) return ServiceResult<User>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");
    
    std::string passwordHash = PasswordHasher::hashPasswordWithSalt(password);
    auto result = userDao->createUser(name, email, passwordHash);

    if(result.isConnectionError()) return ServiceResult<User>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if(result.isInternalError()) return ServiceResult<User>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if(result.isNotFound()) {
        if(result.error_message == "0") return ServiceResult<User>::Fail(ErrorCode::CONFLICT, "邮箱已被注册");
        else if(result.error_message == "1") return ServiceResult<User>::Fail(ErrorCode::CONFLICT, "用户名不可用");
    }
    return ServiceResult<User>::Ok(*result.data, "注册成功");
}

ServiceResult<void> UserService::changePassword(int userId, const std::string& oldPassword, const std::string& newPassword) {
    auto userDao = getUserDao();
    if (!userDao) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");

    auto result = userDao->changePassword(userId, oldPassword, newPassword);

    if(result.isConnectionError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if(result.isInternalError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if(result.isNotFound()) {
        if(result.error_message == "0") return ServiceResult<void>::Fail(ErrorCode::NOT_FOUND, "用户不存在");
        else if(result.error_message == "2") return ServiceResult<void>::Fail(ErrorCode::UNAUTHORIZED, "密码错误");
    }
    return ServiceResult<void>::Ok("密码修改成功");
}

ServiceResult<void> UserService::changeDisplayName(int userId, const std::string& newName) {
    auto userDao = getUserDao();
    if (!userDao) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");

    auto result = userDao->changeDisplayName(userId, newName);

    if(result.isConnectionError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if(result.isInternalError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if(result.isNotFound()) return ServiceResult<void>::Fail(ErrorCode::NOT_FOUND, "用户不存在");
    return ServiceResult<void>::Ok("用户名修改成功");
}

ServiceResult<void> UserService::sendMessage(int userId, int roomId, const std::string& content, const std::string& displayName) {
    auto messageDao = getMessageDao();
    if (!messageDao) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");

    auto result = messageDao->sendMessageToRoom(userId, roomId, content, displayName);

    if(result == MessageOperationResult::SQL_CONNECTION_ERROR) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if(result == MessageOperationResult::DATABASE_INTERNAL_ERROR) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    return ServiceResult<void>::Ok("消息发送成功");
}

ServiceResult<std::vector<Message>> UserService::getMessageHistory(int roomId, int limit) {
    auto messageDao = getMessageDao();
    if (!messageDao) return ServiceResult<std::vector<Message>>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");

    auto result = messageDao->getRecentMessages(roomId, limit);

    if(result.isConnectionError()) return ServiceResult<std::vector<Message>>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if(result.isInternalError()) return ServiceResult<std::vector<Message>>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    return ServiceResult<std::vector<Message>>::Ok(*result.data, "消息获取成功");
}