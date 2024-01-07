cmake_minimum_required(VERSION 3.22)
project(ThreadPool)

set(CMAKE_CXX_STANDARD 23)

include_directories(.)

add_executable(ThreadPool
        example.cpp
        ThreadPool.h)
