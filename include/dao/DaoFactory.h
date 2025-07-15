#pragma once
#include "UserDao.h"
#include "RoomDao.h"
#include "MessageDao.h"
#include <memory>
#include <string>
#include <mutex>

class DaoFactory {
private:
    static DaoFactory* instance_;
    static std::mutex instance_mutex_;
    static std::mutex init_mutex_;
    static std::mutex dao_mutex_;
    static bool initialized_;
    
    DaoFactory() = default;
    DaoFactory(const DaoFactory&) = delete;
    DaoFactory& operator=(const DaoFactory&) = delete;

public:
    static DaoFactory* getInstance();
    static void init();
    static void cleanup();
    
    std::unique_ptr<UserDao> createUserDao();
    std::unique_ptr<RoomDao> createRoomDao();
    std::unique_ptr<MessageDao> createMessageDao();
    
    static UserDao* getUserDao();
    static RoomDao* getRoomDao();
    static MessageDao* getMessageDao();
        
private:
    static UserDao* userDao_;
    static RoomDao* roomDao_;
    static MessageDao* messageDao_;
}; 