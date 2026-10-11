
cmake_minimum_required(VERSION 3.25)

project(input_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(SDL3 REQUIRED CONFIG)
find_package(PkgConfig REQUIRED)

pkg_check_modules(
        LIBEVDEV REQUIRED IMPORTED_TARGET libevdev
)

pkg_check_modules(
        LIBUDEV REQUIRED IMPORTED_TARGET libudev
)

# Application
add_executable(input_viewer
        src/main.cpp
)

target_link_libraries(input_viewer PRIVATE
        SDL3::SDL3
        PkgConfig::LIBEVDEV
        PkgConfig::LIBUDEV
)

target_compile_options(input_viewer PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
