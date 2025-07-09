#pragma once
#include "UserDao.h"
#include "RoomDao.h"
#include <memory>
#include <string>

enum class DaoType {
    MYSQL,
    MEMORY,
};

class DaoFactory {
private:
    static DaoFactory* instance_;
    DaoFactory() = default;
    DaoFactory(const DaoFactory&) = delete;
    DaoFactory& operator=(const DaoFactory&) = delete;

public:
    static DaoFactory* getInstance();
    static void init();
    static void cleanup();
    
    std::unique_ptr<UserDao> createUserDao(DaoType type = DaoType::MYSQL);
    
    std::unique_ptr<RoomDao> createRoomDao(DaoType type = DaoType::MYSQL);
    
    static UserDao* getUserDao();
    
    static RoomDao* getRoomDao();
    
    static void setDefaultType(DaoType type);
    
private:
    static DaoType defaultType_;
    static UserDao* defaultUserDao_;
    static RoomDao* defaultRoomDao_;
}; 