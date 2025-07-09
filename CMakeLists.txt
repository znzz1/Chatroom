cmake_minimum_required(VERSION 3.16)
project(ChatRoom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

include_directories(include)

add_executable(chatroom
    src/main.cpp
    src/net/EpollPoller.cpp
    src/net/Connection.cpp
    src/threadpool/ThreadPool.cpp
    src/dao/DaoFactory.cpp
    src/dao/DatabaseManager.cpp
    src/dao/MySqlUserDao.cpp
    src/dao/MySqlRoomDao.cpp
    src/service/ChatService.cpp
    src/handler/MessageHandler.cpp
    src/utils/PasswordHasher.cpp
)

target_link_libraries(chatroom 
    pthread 
    mysqlclient 
    jsoncpp
    ssl
    crypto
)