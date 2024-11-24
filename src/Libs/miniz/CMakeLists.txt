cmake_minimum_required(VERSION 3.28)
project(miniz)

set(CMAKE_CXX_STANDARD 20)

add_library(miniz miniz.h miniz.c)
target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
