cmake_minimum_required(VERSION 3.10)
project(MultiThreadedBoyerMoore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to an optimized build so timings are meaningful
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Optional GPU backend: configure with -DUSE_CUDA=ON
option(USE_CUDA "Build the CUDA search backend" OFF)

# Add threading support for Phase 2
find_package(Threads REQUIRED)

# Add headers to the include path
include_directories(include)

# The SIMD search uses AVX2 intrinsics
if(MSVC)
    set_source_files_properties(src/simd_search.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2")
else()
    set_source_files_properties(src/simd_search.cpp PROPERTIES COMPILE_FLAGS "-mavx2")
endif()

# Phase 2: Main Executable
add_executable(bm_search
    src/main.cpp
    src/boyer_moore.cpp
    src/thread_pool.cpp
    src/utils.cpp
    src/fuzzy_search.cpp
    src/simd_search.cpp
    src/aho_corasick.cpp)
target_link_libraries(bm_search PRIVATE Threads::Threads)

if(USE_CUDA)
    enable_language(CUDA)
    target_sources(bm_search PRIVATE src/cuda_search.cu)
    target_compile_definitions(bm_search PRIVATE USE_CUDA)
endif()

# Phase 1: Sequential Testing Executable
add_executable(run_tests tests/test_boyer_moore.cpp src/boyer_moore.cpp)
