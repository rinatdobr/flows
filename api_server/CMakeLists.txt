cmake_minimum_required(VERSION 3.15)
project(api_server CXX)

set(CMAKE_CXX_STANDARD 20)

# Make sure you have installed Crow and libpqxx on your system
find_path(CROW_INCLUDE_DIRS "crow.h")
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)

add_executable(api_server
    src/main.cpp
    src/DbPool.cpp)
target_include_directories(api_server PRIVATE ${CROW_INCLUDE_DIRS})
target_link_libraries(api_server PRIVATE
    PkgConfig::PQXX 
    Threads::Threads)
