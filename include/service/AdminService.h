#pragma once
#include "service/BaseService.h"
#include <vector>

class AdminService : public BaseService {
public:
    AdminService();
    ~AdminService() = default;
    
    ServiceResult<Room> createRoom(int adminId, const std::string& name, const std::string& description, int maxUsers = 0);
    ServiceResult<void> deleteRoom(int roomId);
    ServiceResult<void> setRoomName(int roomId, const std::string& name);
    ServiceResult<void> setRoomDescription(int roomId, const std::string& description);
    ServiceResult<void> setRoomMaxUsers(int roomId, int maxUsers);
    ServiceResult<void> setRoomStatus(int roomId, RoomStatus status);  
    ServiceResult<std::vector<Room>> getAllRooms();
}; 