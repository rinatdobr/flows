cmake_minimum_required(VERSION 3.15)
project(api_server CXX)

set(CMAKE_CXX_STANDARD 20)

# Make sure you have installed Crow and libpqxx on your system
find_path(CROW_INCLUDE_DIRS "crow.h")
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)

# Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# ==========================================
# Main executable
# ==========================================
add_executable(api_server
    src/main.cpp
    src/DbPool.cpp
)
target_include_directories(api_server PRIVATE 
    ${CROW_INCLUDE_DIRS}
    src
)
target_link_libraries(api_server PRIVATE
    PkgConfig::PQXX 
    Threads::Threads)

# ==========================================
# Add unit tests
# ==========================================
add_executable(api_server_tests
    tests/test_flows.cpp
    tests/test_auth.cpp
    src/DbPool.cpp
)

target_include_directories(api_server_tests PRIVATE 
    ${CROW_INCLUDE_DIRS}
    src
    tests
)

target_link_libraries(api_server_tests PRIVATE
    PkgConfig::PQXX
    Threads::Threads
    GTest::GTest
    GTest::Main
)

# Enable test discovery
include(GoogleTest)
gtest_discover_tests(api_server_tests)

# Build tests by default
add_custom_target(tests ALL DEPENDS api_server_tests)

# Add a test-only target
add_custom_target(run_tests COMMAND $<TARGET_FILE:api_server_tests> --gtest_brief=1)
add_dependencies(run_tests api_server_tests)
