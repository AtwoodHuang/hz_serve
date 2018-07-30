cmake_minimum_required(VERSION 3.7)
project(hz_server)

set(CMAKE_CXX_STANDARD 11)
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -march=native -O2 -pthread" )
set(SOURCE_FILES memory_pool.cpp memory_pool.h main.cpp hz_error.h epoll.h epoll.cpp priority_queue.h priority_queue.cpp timer.h timer.cpp thread_pool.h thread_pool.cpp http_request.cpp http_request.h list.h other.h http_parse.cpp http_parse.h http.h http.cpp robust_io.h robust_io.cpp other.cpp dynamic_serve.cpp dynamic_serve.h)
add_executable(hz_server ${SOURCE_FILES})