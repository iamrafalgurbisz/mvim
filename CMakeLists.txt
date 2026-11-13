cmake_minimum_required(VERSION 3.16)

project(mvim)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(mvim
    src/main.c
    src/memory/mem_arena.c
)

target_include_directories(mvim PRIVATE
    src
)

target_link_libraries(mvim PRIVATE m)
