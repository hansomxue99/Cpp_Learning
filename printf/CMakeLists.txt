cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 14)

project(printf)

add_compile_options(-Wall -Wextra -Werror=return-type)

add_executable(demo ostream.cpp)