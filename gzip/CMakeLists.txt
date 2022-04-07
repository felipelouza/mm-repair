cmake_minimum_required(VERSION 3.16)
project(ennesimo)

#flags
#if(NOT CMAKE_BUILD_TYPE)
#  set(CMAKE_BUILD_TYPE Release)
#endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

#boost iostream
find_package(Boost 1.36.0 COMPONENTS system iostreams)
include_directories(${Boost_INCLUDE_DIRS})

#pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(test_unco test.cpp)
add_executable(test_gzip test.cpp)

set(GZIP_TARGET test_gzip)
get_target_property(TEMP ${GZIP_TARGET} COMPILE_FLAGS)
if(TEMP STREQUAL "TEMP-NOTFOUND")
    SET(TEMP "") # Set to empty string
else()
    SET(TEMP "${TEMP} ") # A space to cleanly separate from existing content
endif()
# Append our values
SET(TEMP "${TEMP}-DGZIP_ENABLE" )
set_target_properties(${GZIP_TARGET} PROPERTIES COMPILE_FLAGS ${TEMP} )

target_link_libraries(test_unco PRIVATE Threads::Threads)
target_link_libraries(test_gzip PRIVATE Threads::Threads ${Boost_LIBRARIES})