cmake_minimum_required(VERSION 3.10)
project(adblab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(adblab src/main.cpp src/buffer.cpp src/data_storage.cpp src/replacer.cpp)
target_include_directories(adblab PRIVATE include)