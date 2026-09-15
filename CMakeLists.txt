cmake_minimum_required(VERSION 3.10)

# Project name and language
project(GameOfLife LANGUAGES CXX)

# Require at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the executable target
add_executable(game_of_life game_of_life.cpp)

# Optional: print which compiler is being used
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ compiler version: ${CMAKE_CXX_COMPILER_VERSION}")