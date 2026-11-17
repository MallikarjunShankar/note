cmake_minimum_required(VERSION 3.24)
project(Note VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(note
    src/main.cpp
)

target_link_libraries(note PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(note PRIVATE /W4 /permissive-)
else()
    target_compile_options(note PRIVATE -Wall -Wextra -Wpedantic)
endif()
