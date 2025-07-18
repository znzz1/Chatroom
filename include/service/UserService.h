#pragma once
#include "service/BaseService.h"
#include <vector>

class UserService : public BaseService {
public:
    UserService();
    ~UserService() = default;
    
    ServiceResult<User> registerUser(const std::string& email, const std::string& password, const std::string& name);    
    ServiceResult<void> changePassword(int userId, const std::string& oldPassword, const std::string& newPassword);
    ServiceResult<void> changeDisplayName(int userId, const std::string& newName);
    
    ServiceResult<void> sendMessage(int userId, int roomId, const std::string& content, const std::string& displayName);
    ServiceResult<std::vector<Message>> getMessageHistory(int roomId, int limit = 50);
};