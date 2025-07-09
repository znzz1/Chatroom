#include "dao/DaoFactory.h"
#include "dao/MySqlUserDao.h"
#include "dao/MySqlRoomDao.h"
#include <iostream>

DaoFactory* DaoFactory::instance_ = nullptr;
DaoType DaoFactory::defaultType_ = DaoType::MYSQL;
UserDao* DaoFactory::defaultUserDao_ = nullptr;
RoomDao* DaoFactory::defaultRoomDao_ = nullptr;

DaoFactory* DaoFactory::getInstance() {
    if (!instance_) {
        instance_ = new DaoFactory();
    }
    return instance_;
}

void DaoFactory::init() {
    if (instance_) {
        std::cerr << "DaoFactory already initialized." << std::endl;
        return;
    }
    
    instance_ = new DaoFactory();
    
    defaultUserDao_ = instance_->createUserDao(defaultType_).release();
    defaultRoomDao_ = instance_->createRoomDao(defaultType_).release();
    
    std::cout << "DaoFactory initialized with default type: " 
              << (defaultType_ == DaoType::MYSQL ? "MySQL" : "Memory") << std::endl;
}

void DaoFactory::cleanup() {
    if (defaultUserDao_) {
        delete defaultUserDao_;
        defaultUserDao_ = nullptr;
    }
    
    if (defaultRoomDao_) {
        delete defaultRoomDao_;
        defaultRoomDao_ = nullptr;
    }
    
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
    
    std::cout << "DaoFactory cleaned up." << std::endl;
}

std::unique_ptr<UserDao> DaoFactory::createUserDao(DaoType type) {
    switch (type) {
        case DaoType::MYSQL:
            return std::make_unique<MySqlUserDao>();
        case DaoType::MEMORY:
            std::cerr << "MemoryUserDao not implemented yet." << std::endl;
            return nullptr;
        default:
            std::cerr << "Unknown DAO type: " << static_cast<int>(type) << std::endl;
            return nullptr;
    }
}

std::unique_ptr<RoomDao> DaoFactory::createRoomDao(DaoType type) {
    switch (type) {
        case DaoType::MYSQL:
            return std::make_unique<MySqlRoomDao>();
        case DaoType::MEMORY:
            std::cerr << "MemoryRoomDao not implemented yet." << std::endl;
            return nullptr;
        default:
            std::cerr << "Unknown DAO type: " << static_cast<int>(type) << std::endl;
            return nullptr;
    }
}

UserDao* DaoFactory::getUserDao() {
    if (!defaultUserDao_) {
        std::cerr << "UserDao not initialized. Call DaoFactory::init() first." << std::endl;
        return nullptr;
    }
    return defaultUserDao_;
}

RoomDao* DaoFactory::getRoomDao() {
    if (!defaultRoomDao_) {
        std::cerr << "RoomDao not initialized. Call DaoFactory::init() first." << std::endl;
        return nullptr;
    }
    return defaultRoomDao_;
}

void DaoFactory::setDefaultType(DaoType type) {
    defaultType_ = type;
    std::cout << "Default DAO type set to: " 
              << (type == DaoType::MYSQL ? "MySQL" : "Memory") << std::endl;
} 