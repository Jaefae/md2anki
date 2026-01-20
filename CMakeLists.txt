cmake_minimum_required(VERSION 3.16)
project(md2anki LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(md2anki src/main.cpp src/cli.cpp src/parser.cpp)
