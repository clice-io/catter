cmake_minimum_required(VERSION 3.28)
project(modules CXX)

set(CMAKE_CXX_STANDARD 20)
add_executable(modules main.cc)
target_sources(modules PRIVATE FILE_SET CXX_MODULES FILES greet.cppm)
target_compile_definitions(modules PRIVATE CATTER_E2E=1)
