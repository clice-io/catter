cmake_minimum_required(VERSION 3.20)
project(hello CXX)

add_executable(hello src/main.cc src/util.cc)
target_include_directories(hello PRIVATE include)
target_compile_definitions(hello PRIVATE CATTER_E2E=1)
