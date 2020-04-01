cmake_minimum_required(VERSION 2.8.9)
project(timer_unit_test)

include_directories(
        .
        ../
        ../dep/ThreadPool
)

add_executable(timer_unit_test
        timer_unit_test.cpp
        ../manager_timer.cpp)
target_link_libraries(timer_unit_test gtest)
target_link_libraries(timer_unit_test rt)
target_link_libraries(timer_unit_test pthread)