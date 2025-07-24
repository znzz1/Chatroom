#pragma once
#include "service/BaseService.h"
#include "service/UserService.h"
#include "service/AdminService.h"
#include <memory>
#include <vector>

class ServiceManager {
public:
    ServiceManager();
    ~ServiceManager() = default;
    
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;
        
    ServiceResult<User> getUserInfo(int userId);
    ServiceResult<User> registerUser(const std::string& email, const std::string& password, const std::string& name);
    ServiceResult<void> changePassword(const std::string& email, const std::string& oldPassword, const std::string& newPassword);
    ServiceResult<void> changeDisplayName(int userId, const std::string& newName);
    ServiceResult<void> sendMessage(int userId, int roomId, const std::string& content, const std::string& displayName, const std::string& sendTime);
    ServiceResult<std::vector<Message>> getMessageHistory(int roomId, int limit = 50);
    
    ServiceResult<User> login(const std::string& email, const std::string& password);
    ServiceResult<std::vector<Room>> getActiveRooms();  
    ServiceResult<Room> getRoomInfo(int roomId);

    ServiceResult<Room> createRoom(int adminId, const std::string& name, const std::string& description, int maxUsers = 0);
    ServiceResult<void> deleteRoom(int roomId);
    ServiceResult<void> setRoomName(int roomId, const std::string& name);
    ServiceResult<void> setRoomDescription(int roomId, const std::string& description);
    ServiceResult<void> setRoomMaxUsers(int roomId, int maxUsers);
    ServiceResult<void> setRoomStatus(int roomId, bool is_active);
    ServiceResult<std::vector<Room>> getAllRooms();

private:
    UserService user_service_;
    AdminService admin_service_;
}; 