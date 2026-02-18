cmake_minimum_required(VERSION 3.20)

project(cmpe275_mini1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mini1
    src/main.cpp
    src/parsers/CSVParser.cpp
)

target_include_directories(mini1 PRIVATE src)

# Keep Phase 1 simple; enable warnings.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    target_compile_options(mini1 PRIVATE -Wall -Wextra -Wpedantic)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(mini1 PRIVATE -Wall -Wextra -Wpedantic)
endif()
