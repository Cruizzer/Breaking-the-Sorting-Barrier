cmake_minimum_required(VERSION 3.16)
project(SSSP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options for debug/release builds
set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.cpp
    src/sssp_algorithm.cpp
    src/graph.cpp
)

# Find libosmium
find_package(osmium REQUIRED)

# Executable
add_executable(sssp_debug ${SOURCES})

# Link libraries
target_link_libraries(sssp_debug ${OSMIUM_LIBRARIES} z)
