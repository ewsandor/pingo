cmake_minimum_required(VERSION 3.22)

project(Pingo 
        VERSION 0.0.1
        LANGUAGES CXX)

include_directories(inc)

add_library(IPv4 src/ipv4.cpp)
add_library(ICMP src/icmp.cpp)
add_library(PingBlock src/ping_block.cpp)
add_library(PingLogger src/ping_logger.cpp)

add_executable(pingo src/pingo.cpp)
target_link_libraries(pingo PRIVATE IPv4 ICMP PingBlock PingLogger)