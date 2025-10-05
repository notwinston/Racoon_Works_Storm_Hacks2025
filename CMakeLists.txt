cmake_minimum_required(VERSION 3.16)

# Project
project(Racoon_Works_Storm_Hacks2025 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Gurobi setup
set(GUROBI_HOME "/opt/gurobi1100/linux64" CACHE PATH "Path to Gurobi installation")
find_library(GUROBI_CXX_LIBRARY
    NAMES gurobi_c++
    HINTS ${GUROBI_HOME}/lib
    REQUIRED)
find_library(GUROBI_C_LIBRARY
    NAMES gurobi110
    HINTS ${GUROBI_HOME}/lib
    REQUIRED)

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

# Link Gurobi libraries
target_include_directories(scheduler PRIVATE ${GUROBI_HOME}/include)
target_link_libraries(scheduler PRIVATE ${GUROBI_CXX_LIBRARY} ${GUROBI_C_LIBRARY})

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  target_compile_options(scheduler PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(scheduler PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Baseline executable
add_executable(baseline
  src/baseline.cpp
  src/parser.cpp
)
target_include_directories(baseline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  target_compile_options(baseline PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(baseline PRIVATE -Wall -Wextra -Wpedantic)
endif()


