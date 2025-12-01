cmake_minimum_required(VERSION 3.10)

project(ChatClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Boost 库
find_package(Boost REQUIRED)

add_executable(client base.cpp)

# 链接 Boost 和 Windows 网络库
if(WIN32)
    target_link_libraries(client PRIVATE ws2_32 mswsock)
endif()

target_link_libraries(client PRIVATE Boost::boost)
