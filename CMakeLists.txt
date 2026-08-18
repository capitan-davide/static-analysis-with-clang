cmake_minimum_required(VERSION 4.0)
project(csa-code-examples VERSION 0.1.0 LANGUAGES C CXX)

find_package(Clang REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(MISRA MODULE
  Rule22_04Checker.cpp
)

target_include_directories(MISRA
  PRIVATE ${CLANG_INCLUDE_DIRS}
)
