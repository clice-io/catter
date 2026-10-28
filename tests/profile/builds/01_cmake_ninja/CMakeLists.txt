cmake_minimum_required(VERSION 3.20)
project(hello CXX)

add_library(util STATIC src/util.cc)
target_include_directories(util PUBLIC include)
target_compile_definitions(util PUBLIC CATTER_TEST=1)

add_executable(hello src/main.cc)
target_link_libraries(hello PRIVATE util)
