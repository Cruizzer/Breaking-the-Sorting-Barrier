cmake_minimum_required(VERSION 3.15)
project(breaking_the_sorting_barrier CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/graph_generator.cpp
    src/algorithms/dijkstra.cpp
    src/algorithms/bmssp.cpp
    src/benchmark/benchmark.cpp
)

# Create executable
add_executable(sssp ${SOURCES})

# Compiler optimizations for Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(sssp PRIVATE -O3 -march=native)
endif()
