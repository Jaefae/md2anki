cmake_minimum_required(VERSION 3.16)
project(md2anki LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_TESTS "Build unit tests" OFF)

file(GLOB LIB_SOURCES "src/*.cpp")
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(md2anki_lib STATIC ${LIB_SOURCES})
target_include_directories(md2anki_lib PUBLIC include third_party)

if(WIN32)
  target_link_libraries(md2anki_lib PUBLIC ws2_32)
endif()

add_executable(md2anki src/main.cpp)
target_link_libraries(md2anki PRIVATE md2anki_lib)

if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
