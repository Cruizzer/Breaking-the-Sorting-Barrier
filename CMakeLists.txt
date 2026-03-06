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

# Find dependencies
include(FetchContent)

# Catch2 for unit testing
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# Google Benchmark
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

# Common source files (shared by all targets)
set(COMMON_SOURCES
    src/graph_generator.cpp
    src/algorithms/dijkstra.cpp
    src/algorithms/bmssp.cpp
    src/algorithms/bmssp_expected.cpp
    src/algorithms/bmssp_wc.cpp
)

# Main executable
set(MAIN_SOURCES
    src/main.cpp
    src/benchmark/benchmark.cpp
)

add_executable(sssp ${MAIN_SOURCES} ${COMMON_SOURCES})

# Compiler optimizations for Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(sssp PRIVATE -O3 -march=native)
endif()

# ==================== Unit Tests ====================
enable_testing()

set(TEST_SOURCES
    test/test_main.cpp
    test/test_graph.cpp
    test/test_dijkstra.cpp
    test/test_bmssp.cpp
)

add_executable(unit_tests ${TEST_SOURCES} ${COMMON_SOURCES})
target_link_libraries(unit_tests PRIVATE Catch2::Catch2)

# Register tests with CTest
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(unit_tests)

# ==================== Google Benchmarks ====================

add_executable(gbench benchmarks_google/benchmark_algorithms.cpp ${COMMON_SOURCES})
target_link_libraries(gbench PRIVATE benchmark::benchmark)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(gbench PRIVATE -O3 -march=native)
endif()

# ==================== Custom Targets ====================

# Run unit tests
add_custom_target(test_run
    COMMAND unit_tests
    DEPENDS unit_tests
    COMMENT "Running unit tests..."
)

# Run benchmarks
add_custom_target(benchmark_run
    COMMAND gbench --benchmark_out=benchmark_results.json --benchmark_out_format=json
    DEPENDS gbench
    COMMENT "Running Google Benchmarks..."
)

# Run all tests and benchmarks
add_custom_target(test_all
    COMMAND unit_tests
    COMMAND gbench
    DEPENDS unit_tests gbench
    COMMENT "Running all tests and benchmarks..."
)
