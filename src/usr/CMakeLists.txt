cmake_minimum_required(VERSION 3.10)
project(usr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(SOURCES main.cpp)

add_executable(usr ${SOURCES})

target_include_directories(usr PRIVATE ../../3rd-party/kstl)