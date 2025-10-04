cmake_minimum_required(VERSION 3.16)

# Project
project(Racoon_Works_Storm_Hacks2025 LANGUAGES CXX)

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

# Target: scheduler (new src-based build)
add_executable(scheduler
  src/main.cpp
  src/parser.cpp
  src/scheduler.cpp
)
target_include_directories(scheduler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  target_compile_options(scheduler PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(scheduler PRIVATE -Wall -Wextra -Wpedantic)
endif()


