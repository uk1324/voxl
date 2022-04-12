cmake_minimum_required(VERSION 3.15)

project(voxl)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)
add_subdirectory(src)
# Working directory can be set by clicking debug > debug and launch settings for <target> and settings "currentDir": "${projectDir}"
add_executable(voxl-run "src/main.cpp")
target_link_libraries(voxl-run voxl-lib)

add_subdirectory(test)