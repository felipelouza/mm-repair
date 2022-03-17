project(block_gzip)

cmake_minimum_required(VERSION 3.0)

#boost iostream
find_package(Boost 1.36.0 COMPONENTS system iostreams)
include_directories(${Boost_INCLUDE_DIRS})

#pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(block_gzip block_gzip.cpp)
add_executable(test test.cpp)

target_link_libraries(block_gzip PRIVATE ${Boost_LIBRARIES})
target_link_libraries(test PRIVATE Threads::Threads ${Boost_LIBRARIES})