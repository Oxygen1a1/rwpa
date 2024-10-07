cmake_minimum_required(VERSION 3.10)
project(drv LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

set(SOURCES
    main.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /EHs-")
wdk_add_driver(drv ${SOURCES})
target_include_directories(drv PRIVATE ../../3rd-party/kstl)