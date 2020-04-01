cmake_minimum_required(VERSION 2.8.9)
project(timer)

if(UNIT_TEST)
    add_subdirectory(timer_unit_test)
endif()

include_directories(
        .
        ./dep/ThreadPool
)

add_library(manager_timer manager_timer.cpp)
add_executable(demo demo.cpp)
target_link_libraries(demo manager_timer)
target_link_libraries(demo rt)
target_link_libraries(demo pthread)