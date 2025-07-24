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

file(GLOB_RECURSE ALL_SOURCES "src/*.cpp")

list(REMOVE_ITEM ALL_SOURCES "${CMAKE_SOURCE_DIR}/src/server/main.cpp")

add_library(chatroom_service_lib STATIC ${ALL_SOURCES})

target_link_libraries(chatroom_service_lib 
    pthread 
    mysqlclient 
    jsoncpp
    ssl
    crypto
)

add_executable(ChatRoomServer src/server/main.cpp)
target_link_libraries(ChatRoomServer chatroom_service_lib pthread mysqlclient jsoncpp ssl crypto)