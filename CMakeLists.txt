cmake_minimum_required(VERSION 3.20)
project(dining_philosophers_problem CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
