cmake_minimum_required(VERSION 3.15)
project(sssp_osm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Add your include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include       # your graph.hpp etc
    ${PROJECT_SOURCE_DIR}/external/libosmium/include
    ${PROJECT_SOURCE_DIR}/include/protozero/include
)

# Find system libraries
find_package(ZLIB REQUIRED)
find_package(EXPAT REQUIRED)
find_package(BZip2)   # optional
find_package(Threads REQUIRED)

# Your sources
set(SOURCES
    src/main.cpp
    src/sssp_algorithm.cpp
    src/osm_loader.cpp
)

add_executable(sssp ${SOURCES})

# Link libraries
target_link_libraries(sssp
    PRIVATE
        ZLIB::ZLIB
        EXPAT::EXPAT
        ${BZIP2_LIBRARIES}   # only if needed
        Threads::Threads
)
