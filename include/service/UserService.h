#pragma once
#include "service/BaseService.h"

class UserService : public BaseService {
public:
    UserService();
    ~UserService() = default;
    
    ServiceResult<User> registerUser(const std::string& email, const std::string& password, const std::string& name);    
    ServiceResult<void> changePassword(const std::string& email, const std::string& oldPassword, const std::string& newPassword);
    ServiceResult<void> changeDisplayName(int userId, const std::string& newName);
    ServiceResult<void> sendMessage(int userId, int roomId, const std::string& content, const std::string& displayName, const std::string& sendTime);
    ServiceResult<std::vector<Message>> getMessageHistory(int roomId, int limit = 50);
};