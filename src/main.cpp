#include "server/ChatRoomServer.h"
#include "dao/DaoFactory.h"
#include "database/DatabaseManager.h"
#include <iostream>

int main() {
    try {
        // 初始化数据库连接池
        DatabaseManager::init("localhost", 3306, "znzz1", "20010717zn", "chatroom", 5, 20);
        
        // 初始化DAO工厂
        DaoFactory::init();
        
        // 创建并启动聊天服务器
        ChatRoomServer server(12345);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    // 清理资源
    DaoFactory::cleanup();
    DatabaseManager::cleanup();
    
    return 0;
}
