cmake_minimum_required(VERSION 4.1)
project(dining_philosophers_problem)

set(CMAKE_CXX_STANDARD 20)

add_executable(dining_philosophers_problem
        main.cpp
        common.cpp
        philosophers_hierarchy.cpp
        philosophers_waiter.cpp
        philosophers_fair.cpp
        philosophers_deadlock.cpp)

target_compile_options(dining_philosophers_problem PRIVATE -Wall -Wextra)

find_package(Threads REQUIRED)
target_link_libraries(dining_philosophers_problem PRIVATE Threads::Threads)
