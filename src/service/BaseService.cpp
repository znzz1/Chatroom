#include "service/BaseService.h"
#include <iostream>

BaseService::BaseService() {
}

ServiceResult<User> BaseService::getUserInfo(int userId) {
    auto userDao = getUserDao();
    if (!userDao) return ServiceResult<User>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");

    auto userResult = userDao->getUserById(userId);
    if (userResult.isConnectionError()) return ServiceResult<User>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (userResult.isInternalError()) return ServiceResult<User>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if (userResult.isNotFound()) return ServiceResult<User>::Fail(ErrorCode::NOT_FOUND, "用户不存在");

    return ServiceResult<User>::Ok(*userResult.data, "获取用户信息成功");
}

ServiceResult<User> BaseService::login(const std::string& email, const std::string& password) {
    auto userDao = getUserDao();
    if (!userDao) return ServiceResult<User>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");

    auto userResult = userDao->authenticateUser(email, password);
    
    if(userResult.isConnectionError()) return ServiceResult<User>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if(userResult.isInternalError()) return ServiceResult<User>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if(userResult.isNotFound()) {
        if(userResult.error_message == "0") return ServiceResult<User>::Fail(ErrorCode::NOT_FOUND, "用户不存在");
        else if(userResult.error_message == "2") return ServiceResult<User>::Fail(ErrorCode::UNAUTHORIZED, "密码错误");
    }

    return ServiceResult<User>::Ok(*userResult.data, "登录成功");
}

ServiceResult<std::vector<Room>> BaseService::getActiveRooms() {
    auto roomDao = getRoomDao();
    if (!roomDao) return ServiceResult<std::vector<Room>>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");
    
    auto roomsResult = roomDao->getActiveRooms();

    if (roomsResult.isConnectionError()) return ServiceResult<std::vector<Room>>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (roomsResult.isInternalError()) return ServiceResult<std::vector<Room>>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    
    return ServiceResult<std::vector<Room>>::Ok(*roomsResult.data, "获取活跃房间列表成功");
}

ServiceResult<Room> BaseService::getRoomInfo(int roomId) {
    auto roomDao = getRoomDao();
    if (!roomDao) return ServiceResult<Room>::Fail(ErrorCode::INTERNAL_ERROR, "DAO层初始化失败");
    
    auto roomResult = roomDao->getRoomById(roomId);
    if (roomResult.isConnectionError()) return ServiceResult<Room>::Fail(ErrorCode::INTERNAL_ERROR, "数据库连接失败");
    if (roomResult.isInternalError()) return ServiceResult<Room>::Fail(ErrorCode::INTERNAL_ERROR, "数据库内部错误");
    if (roomResult.isNotFound()) return ServiceResult<Room>::Fail(ErrorCode::NOT_FOUND, "房间不存在");
    
    return ServiceResult<Room>::Ok(*roomResult.data, "获取房间信息成功");
} 