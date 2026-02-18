cmake_minimum_required(VERSION 3.14)
project(OrderBook CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main demo
add_executable(orderbook src/main.cpp src/OrderBook.cpp)
target_include_directories(orderbook PRIVATE src)

# Benchmark
add_executable(benchmark bench/Benchmark.cpp src/OrderBook.cpp)
target_include_directories(benchmark PRIVATE src)

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(tests tests/TestOrderBook.cpp src/OrderBook.cpp)
target_include_directories(tests PRIVATE src)
target_link_libraries(tests GTest::gtest_main)
add_test(NAME OrderBookTests COMMAND tests)
