#include "dao/DaoFactory.h"
#include "dao/MySqlUserDao.h"
#include "dao/MySqlRoomDao.h"
#include "dao/MySqlMessageDao.h"
#include <iostream>
#include <mutex>

DaoFactory* DaoFactory::instance_ = nullptr;
UserDao* DaoFactory::userDao_ = nullptr;
RoomDao* DaoFactory::roomDao_ = nullptr;
MessageDao* DaoFactory::messageDao_ = nullptr;
std::mutex DaoFactory::instance_mutex_;
std::mutex DaoFactory::init_mutex_;
std::mutex DaoFactory::dao_mutex_;
bool DaoFactory::initialized_ = false;

DaoFactory* DaoFactory::getInstance() {
    if (!instance_) {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_) {
            instance_ = new DaoFactory();
        }
    }
    return instance_;
}

void DaoFactory::init() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    
    if (initialized_) {
        std::cerr << "DaoFactory already initialized." << std::endl;
        return;
    }
    
    if (!instance_) {
        instance_ = new DaoFactory();
    }
    
    userDao_ = instance_->createUserDao().release();
    roomDao_ = instance_->createRoomDao().release();
    messageDao_ = instance_->createMessageDao().release();
    initialized_ = true;
}

void DaoFactory::cleanup() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    
    if (userDao_) {
        delete userDao_;
        userDao_ = nullptr;
    }
    
    if (roomDao_) {
        delete roomDao_;
        roomDao_ = nullptr;
    }
    
    if (messageDao_) {
        delete messageDao_;
        messageDao_ = nullptr;
    }
    
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
    
    initialized_ = false;
}

std::unique_ptr<UserDao> DaoFactory::createUserDao() {
    return std::make_unique<MySqlUserDao>();
}

std::unique_ptr<RoomDao> DaoFactory::createRoomDao() {
    return std::make_unique<MySqlRoomDao>();
}

std::unique_ptr<MessageDao> DaoFactory::createMessageDao() {
    return std::make_unique<MySqlMessageDao>();
}

UserDao* DaoFactory::getUserDao() {
    std::lock_guard<std::mutex> lock(dao_mutex_);
    if (!initialized_) {
        std::cerr << "UserDao not initialized. Call DaoFactory::init() first." << std::endl;
        return nullptr;
    }
    return userDao_;
}

RoomDao* DaoFactory::getRoomDao() {
    std::lock_guard<std::mutex> lock(dao_mutex_);
    if (!initialized_) {
        std::cerr << "RoomDao not initialized. Call DaoFactory::init() first." << std::endl;
        return nullptr;
    }
    return roomDao_;
}

MessageDao* DaoFactory::getMessageDao() {
    std::lock_guard<std::mutex> lock(dao_mutex_);
    if (!initialized_) {
        std::cerr << "MessageDao not initialized. Call DaoFactory::init() first." << std::endl;
        return nullptr;
    }
    return messageDao_;
}