cmake_minimum_required(VERSION 3.0.2)
project (separate_chaining)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

set(CXX_STANDARD c++17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -march=native -std=${CXX_STANDARD} ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mtune=native")


 
##########
# library
##########

install(FILES separate_chaining_map.hpp DESTINATION include)


##########
# examples
##########

#add_executable  (example_map example_map.cpp)


##########
# TESTS
##########

find_package(GTest)
include_directories(${GTEST_INCLUDE_DIR})

add_executable  (test_map test_map.cpp)

target_link_libraries(test_map gtest pthread)

enable_testing()
add_test(NAME map COMMAND test_map)


MESSAGE( STATUS "With Gtest?: " ${GTEST_INCLUDE_DIR} )
MESSAGE( STATUS "Built Type: " ${CMAKE_BUILD_TYPE} )
MESSAGE( STATUS "CXX Flags: " ${CMAKE_CXX_FLAGS} )