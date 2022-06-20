cmake_minimum_required(VERSION 3.22)
project(os_hw3)

set(CMAKE_CXX_STANDARD 20)

add_executable(os_hw3 main.cpp parser.c fat32.h parser.h)
