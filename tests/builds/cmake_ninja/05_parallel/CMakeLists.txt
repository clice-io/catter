cmake_minimum_required(VERSION 3.20)
project(many C CXX)

file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*")
add_library(many STATIC ${SOURCES})
target_compile_definitions(many PRIVATE CATTER_TEST=1)
