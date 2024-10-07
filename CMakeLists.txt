cmake_minimum_required(VERSION 3.10)
project(CfCheater LANGUAGES CXX)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
if(NOT WDK_FOUND)
    find_package(WDK REQUIRED)
endif()

add_subdirectory(./src/usr)
add_subdirectory(./src/drv)