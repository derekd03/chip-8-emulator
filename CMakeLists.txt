cmake_minimum_required(VERSION 3.10)
project(chip8-emulator)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)

file(GLOB SOURCES src/*.cpp)

add_executable(chip8-emulator ${SOURCES})

target_include_directories(chip8-emulator PRIVATE src)
target_link_libraries(chip8-emulator PRIVATE SDL2::SDL2main SDL2::SDL2)