cmake_minimum_required(VERSION 3.0.2)
project (separate_chaining)


if(NOT CMAKE_BUILD_TYPE)
    #set(CMAKE_BUILD_TYPE "Release")
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

set(CXX_STANDARD gnu++17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -march=native -std=${CXX_STANDARD} ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -ggdb -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mtune=native")

include_directories(include)
add_subdirectory(external/bit_span)
include_directories(external/bit_span/include)
include_directories(external/broadwordsearch/include)


##########
# library
##########

install(FILES separate_chaining_map.hpp DESTINATION include)


##########
# examples
##########

add_executable  (example_map example_map.cpp)
target_link_libraries(example_map gtest pthread ${GLOG_LIBRARY})


##########
# TESTS
##########

FIND_LIBRARY(GLOG_LIBRARY glog)
FIND_PATH(GLOG_INCLUDE_DIR "glog/logging.h")

find_package(GTest)
include_directories(${GTEST_INCLUDE_DIR})

add_executable  (test_map test_map.cpp)

target_link_libraries(test_map gtest pthread ${GLOG_LIBRARY})

enable_testing()
add_test(NAME map COMMAND test_map)


MESSAGE( STATUS "With Gtest?: " ${GTEST_INCLUDE_DIR} )
MESSAGE( STATUS "Built Type: " ${CMAKE_BUILD_TYPE} )
MESSAGE( STATUS "CXX Flags: " ${CMAKE_CXX_FLAGS} )
