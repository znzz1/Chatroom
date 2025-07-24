#pragma once
#include <memory>
#include <string>
#include <vector>
#include "dao/DaoFactory.h"
#include "models/User.h"
#include "models/Room.h"
#include "models/Message.h"

enum class ErrorCode {
    SUCCESS = 200,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    CONFLICT = 409,
    INTERNAL_ERROR = 500
};

template<typename T>
struct ServiceResult {
    bool ok;
    ErrorCode code;
    std::string message;
    T data;

    static ServiceResult<T> Ok(const T& d, const std::string& msg = "Success") { 
        return {true, ErrorCode::SUCCESS, msg, d}; 
    }
    static ServiceResult<T> Fail(ErrorCode c, const std::string& msg) { 
        return {false, c, msg, T{}}; 
    }
};

template<>
struct ServiceResult<void> {
    bool ok;
    ErrorCode code;
    std::string message;

    static ServiceResult<void> Ok(const std::string& msg = "Success") { 
        return {true, ErrorCode::SUCCESS, msg}; 
    }
    static ServiceResult<void> Fail(ErrorCode c, const std::string& msg) { 
        return {false, c, msg}; 
    }
};

class BaseService {
public:
    BaseService();
    ~BaseService() = default;
    
    ServiceResult<User> getUserInfo(int userId);
    ServiceResult<User> login(const std::string& email, const std::string& password);
    ServiceResult<std::vector<Room>> getActiveRooms();
    ServiceResult<Room> getRoomInfo(int roomId);
    
protected:
    UserDao* getUserDao() { return DaoFactory::getUserDao(); }
    RoomDao* getRoomDao() { return DaoFactory::getRoomDao(); }
    MessageDao* getMessageDao() { return DaoFactory::getMessageDao(); }
}; 