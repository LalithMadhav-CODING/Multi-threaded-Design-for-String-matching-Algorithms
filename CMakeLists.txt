cmake_minimum_required(VERSION 3.10)
project(MultiThreadedBoyerMoore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add threading support for Phase 2
find_package(Threads REQUIRED)

# Add headers to the include path
include_directories(include)

# Phase 2: Main Executable
add_executable(bm_search src/main.cpp src/boyer_moore.cpp src/thread_pool.cpp src/utils.cpp)
target_link_libraries(bm_search PRIVATE Threads::Threads)

# Phase 1: Sequential Testing Executable
add_executable(run_tests tests/test_boyer_moore.cpp src/boyer_moore.cpp)
