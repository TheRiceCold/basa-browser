cmake_minimum_required(VERSION 3.20)

project(basa-browser
    VERSION 0.1.0
    DESCRIPTION "Linux-based remote browser management system"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
)

# Main library
add_library(basa_browser
    src/process.cpp
)

target_include_directories(basa_browser
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# Executable
add_executable(basa-browser
    src/main.cpp
)

target_link_libraries(basa-browser
    PRIVATE basa_browser
)
