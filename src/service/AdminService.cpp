#include "service/AdminService.h"
#include <iostream>

AdminService::AdminService() {
}

ServiceResult<Room> AdminService::createRoom(int adminId, const std::string& name, const std::string& description, int maxUsers) {
    auto roomDao = getRoomDao();
    if (!roomDao) return ServiceResult<Room>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");
    
    auto createRoomResult = roomDao->createRoom(adminId, name, description, maxUsers);

    if (createRoomResult.isConnectionError()) return ServiceResult<Room>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (createRoomResult.isInternalError()) return ServiceResult<Room>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    
    return ServiceResult<Room>::Ok(*createRoomResult.data, "房间创建成功");
}

ServiceResult<void> AdminService::deleteRoom(int roomId) {
    auto roomDao = getRoomDao();
    if (!roomDao) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");

    auto deleteResult = roomDao->deleteRoom(roomId);

    if (deleteResult.isConnectionError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (deleteResult.isInternalError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if (deleteResult.isNotFound()) return ServiceResult<void>::Fail(ErrorCode::NOT_FOUND, "房间不存在");

    return ServiceResult<void>::Ok("房间删除成功");
}

ServiceResult<void> AdminService::setRoomStatus(int roomId, RoomStatus status) {
    auto roomDao = getRoomDao();
    if (!roomDao) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");
    
    auto updateResult = roomDao->setRoomStatus(roomId, status); 

    if (updateResult.isConnectionError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (updateResult.isInternalError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if (updateResult.isNotFound()) return ServiceResult<void>::Fail(ErrorCode::NOT_FOUND, "房间不存在");
    
    return ServiceResult<void>::Ok("房间状态更新成功");
}

ServiceResult<void> AdminService::setRoomName(int roomId, const std::string& name) {
    auto roomDao = getRoomDao();
    if (!roomDao) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");
    
    auto updateResult = roomDao->setRoomName(roomId, name);

    if (updateResult.isConnectionError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (updateResult.isInternalError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if (updateResult.isNotFound()) return ServiceResult<void>::Fail(ErrorCode::NOT_FOUND, "房间不存在");
    
    return ServiceResult<void>::Ok("房间名称更新成功");
}

ServiceResult<void> AdminService::setRoomDescription(int roomId, const std::string& description) {
    auto roomDao = getRoomDao();
    if (!roomDao) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");
    
    auto updateResult = roomDao->setRoomDescription(roomId, description);

    if (updateResult.isConnectionError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (updateResult.isInternalError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if (updateResult.isNotFound()) return ServiceResult<void>::Fail(ErrorCode::NOT_FOUND, "房间不存在");
    
    return ServiceResult<void>::Ok("房间描述更新成功");
}

ServiceResult<void> AdminService::setRoomMaxUsers(int roomId, int maxUsers) {
    auto roomDao = getRoomDao();
    if (!roomDao) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");
    
    auto updateResult = roomDao->setRoomMaxUsers(roomId, maxUsers);

    if (updateResult.isConnectionError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (updateResult.isInternalError()) return ServiceResult<void>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if (updateResult.isNotFound()) return ServiceResult<void>::Fail(ErrorCode::NOT_FOUND, "房间不存在");
    
    return ServiceResult<void>::Ok("最大用户数更新成功");
}

ServiceResult<std::vector<Room>> AdminService::getAllRooms() {
    auto roomDao = getRoomDao();
    if (!roomDao) return ServiceResult<std::vector<Room>>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");
    
    auto roomsResult = roomDao->getAllRooms();

    if (roomsResult.isConnectionError()) return ServiceResult<std::vector<Room>>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (roomsResult.isInternalError()) return ServiceResult<std::vector<Room>>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");

    return ServiceResult<std::vector<Room>>::Ok(*roomsResult.data, "获取房间列表成功");
} 