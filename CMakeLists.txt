cmake_minimum_required(VERSION 3.0.2)
project (separate_chaining)


if(NOT CMAKE_BUILD_TYPE)
    #set(CMAKE_BUILD_TYPE "Release")
    set(CMAKE_BUILD_TYPE "Debug")
endif(NOT CMAKE_BUILD_TYPE)

set(CXX_STANDARD c++17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -march=native -std=${CXX_STANDARD} ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -ggdb -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mtune=native")

add_subdirectory(external/bit_span)
include_directories(external/bit_span/include)
include_directories(external/tudostats/include)


add_subdirectory(external/tudostats)

##########
# BENCHMARKS
##########

set(NO_CELERO 0)
find_path(CELERO_INCLUDE_DIR celero/Celero.h)
find_library(CELERO_LIB libcelero.so)
if(CELERO_INCLUDE_DIR STREQUAL "CELERO_INCLUDE_DIR-NOTFOUND" OR CELERO_LIB_DIR STREQUAL "CELERO_LIB-NOTFOUND")
	set(NO_CELERO 1)
	add_definitions(-DNO_CELERO=1)
else()
	include_directories(${CELERO_INCLUDE_DIR})
endif()

if(NO_CELERO EQUAL 0)
	add_executable(bench_table bench_table.cpp)
	target_link_libraries(bench_table ${CELERO_LIB})
endif()

##########
# stats
##########

add_executable  (stats_hashtable stats_hashtable.cpp)

target_link_libraries(stats_hashtable tudocomp_stat)
 
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


MESSAGE( STATUS "With Celero?: " ${CELERO_INCLUDE_DIR} ${CELERO_LIB_DIR} )
MESSAGE( STATUS "With Gtest?: " ${GTEST_INCLUDE_DIR} )
MESSAGE( STATUS "Built Type: " ${CMAKE_BUILD_TYPE} )
MESSAGE( STATUS "CXX Flags: " ${CMAKE_CXX_FLAGS} )
