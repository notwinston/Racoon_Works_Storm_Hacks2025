cmake_minimum_required(VERSION 3.16)

# Project
project(Parser_Test LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# On macOS, explicitly choose clang++ and libc++ if needed
if(APPLE)
  if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE STRING "C++ compiler" FORCE)
  endif()
  add_compile_options(-fcolor-diagnostics -fansi-escape-codes)
  add_link_options(-stdlib=libc++)
endif()

# Test parser executable
add_executable(test_parser
  ../src/test_parser.cpp
  ../src/parser.cpp
)
target_include_directories(test_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  target_compile_options(test_parser PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(test_parser PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Custom target to clean build directory
add_custom_target(clean_build
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Removing build directory: ${CMAKE_BINARY_DIR}"
)
