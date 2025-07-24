#include "database/DatabaseManager.h"
#include "dao/DaoFactory.h"
#include "server/ChatRoomServer.h"
#include "utils/EnvLoader.h"
#include <iostream>

int main() {
    try {
        EnvLoader::loadFromFile(".env");
        DatabaseManager::init();
        DaoFactory::init();
        ChatRoomServer server;
        server.run();
    } catch (const std::exception& ex) {
        std::cerr << "服务器启动失败: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}