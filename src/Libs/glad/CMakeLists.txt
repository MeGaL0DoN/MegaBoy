cmake_minimum_required(VERSION 3.28)
project(glad)

set(CMAKE_CXX_STANDARD 20)

add_library(glad glad.h glad.c KHR/khrplatform.h)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
