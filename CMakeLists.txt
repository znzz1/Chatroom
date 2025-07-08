cmake_minimum_required(VERSION 3.20)
project(ChatRoom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_executable(chatroom_epoll
    src/net/main_epoll.cpp
    src/net/EpollPoller.cpp
)
target_include_directories(chatroom_epoll PUBLIC include)
target_link_libraries(chatroom_epoll PRIVATE pthread)