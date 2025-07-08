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

include_directories(include)

add_executable(chatroom_epoll
    src/net/main_epoll.cpp
    src/net/EpollPoller.cpp
)
target_link_libraries(chatroom_epoll PRIVATE pthread)

add_executable(chatroom_threadpool
    src/net/main_threadpool.cpp
    src/net/EpollPoller.cpp
    src/net/Connection.cpp
    src/threadpool/ThreadPool.cpp
)
target_link_libraries(chatroom_threadpool PRIVATE pthread)