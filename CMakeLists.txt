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
target_link_libraries(example_map glog pthread ${GLOG_LIBRARY})

##########
# glog
##########

# FIND_LIBRARY(GLOG_LIBRARY glog)
# FIND_PATH(GLOG_INCLUDE_DIR "glog/logging.h")
if(NOT Glog_FOUND)
    add_subdirectory(external/glog)
    include_directories(external/glog/)
endif(NOT Glog_FOUND)

##########
# TESTS
##########

find_package(GTest)
if(GTEST_FOUND)
    include_directories(${GTEST_INCLUDE_DIR})

    file(GLOB_RECURSE testsources RELATIVE ${CMAKE_SOURCE_DIR} "test/*.cpp")

    enable_testing()

    foreach(testsource ${testsources})
	string( REPLACE ".cpp" "" testpath ${testsource} )
	get_filename_component(testname ${testpath} NAME)
	add_executable(test_${testname} ${testsource})
	target_include_directories(test_${testname} PUBLIC test)
	target_link_libraries(test_${testname} gtest pthread ${GLOG_LIBRARY})
	add_test(NAME ${testname} COMMAND test_${testname})
    endforeach()
endif(GTEST_FOUND)


MESSAGE( STATUS "With Gtest?: " ${GTEST_INCLUDE_DIR} )
MESSAGE( STATUS "Built Type: " ${CMAKE_BUILD_TYPE} )
MESSAGE( STATUS "CXX Flags: " ${CMAKE_CXX_FLAGS} )
