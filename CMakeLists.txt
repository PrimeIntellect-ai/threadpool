cmake_minimum_required(VERSION 3.30)
project(threadpool)

set(CMAKE_CXX_STANDARD 20)

add_library(threadpool STATIC src/threadpool.cpp)

target_include_directories(threadpool PUBLIC include)
target_include_directories(threadpool PRIVATE internal)

add_subdirectory(third_party)

target_link_libraries(threadpool PRIVATE threadpark)

option(PI_THREADPOOL_BUILD_TESTS "Build tests" OFF)

enable_testing()
if (PI_THREADPOOL_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
