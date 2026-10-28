cmake_minimum_required(VERSION 3.20)
project(util CXX)

add_library(util STATIC src/util.cc src/twice.cc)
target_include_directories(util PUBLIC include)
target_compile_definitions(util PUBLIC CATTER_TEST=1)
