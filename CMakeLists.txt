cmake_minimum_required(VERSION 3.16)
project(md2anki LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB SOURCES "src/*.cpp")
add_executable(md2anki ${SOURCES})

target_include_directories(md2anki PRIVATE include)
