cmake_minimum_required(VERSION 3.22)

project(Pingo 
        VERSION 0.0.1
        LANGUAGES CXX)

add_library(IPv4 ipv4.cpp)
add_library(ICMP icmp.cpp)
add_library(PingBlock ping_block.cpp)
add_library(PingLogger ping_logger.cpp)

add_executable(pingo pingo.cpp)
target_link_libraries(pingo PRIVATE IPv4 ICMP PingBlock PingLogger)