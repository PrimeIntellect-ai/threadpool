add_executable(basic_sched_test basic_sched_test.cpp)
target_link_libraries(basic_sched_test PRIVATE threadpool)
add_test(NAME basic_sched_test COMMAND basic_sched_test)