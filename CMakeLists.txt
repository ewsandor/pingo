cmake_minimum_required(VERSION 3.13)

project(Pingo 
        VERSION 0.0.1
        DESCRIPTION "A project to ping every single IPv4 address to experiment with basic networking in Linux. (ICMP, ip headers, raw sockets, etc)"
        LANGUAGES CXX
        HOMEPAGE_URL "https://git.sandorlaboratories.com/edward/pingo")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)


add_compile_options(-Wall -Wextra -Werror)

configure_file(inc/version.hpp.in version.hpp @ONLY)
include_directories(${PROJECT_BINARY_BIN})

include_directories(inc ${CMAKE_CURRENT_BINARY_DIR})

add_library(File       OBJECT src/file.cpp)
add_library(Hilbert    OBJECT src/hilbert.cpp)
add_library(IPv4       OBJECT src/ipv4.cpp)
add_library(ICMP       OBJECT src/icmp.cpp)
add_library(PingBlock  OBJECT src/ping_block.cpp)
add_library(PingLogger OBJECT src/ping_logger.cpp)

add_executable(pingo src/pingo.cpp)
target_link_libraries(pingo PRIVATE OpenSSL::SSL Threads::Threads File Hilbert IPv4 ICMP PingBlock PingLogger)