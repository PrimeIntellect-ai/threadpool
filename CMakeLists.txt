cmake_minimum_required(VERSION 3.16)
project(threadpool)

set(CMAKE_CXX_STANDARD 20)

add_library(threadpool STATIC src/threadpool.cpp)

target_include_directories(threadpool PUBLIC include)
target_include_directories(threadpool PRIVATE internal)

add_subdirectory(third_party)
target_link_libraries(threadpool PRIVATE threadpark)

find_package(Threads REQUIRED)
target_link_libraries(threadpool PRIVATE Threads::Threads)

option(PI_THREADPOOL_BUILD_TESTS "Build tests" ON)

enable_testing()
if (PI_THREADPOOL_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
