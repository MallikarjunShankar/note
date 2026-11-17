cmake_minimum_required(VERSION 3.24)
project(NoteEditorPrototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(buffer_bench buffer_bench.cpp text_buffers.cpp)
target_compile_options(buffer_bench PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O2;-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/O2;/W4;/permissive->
)
enable_testing()
add_test(NAME buffer_self_test COMMAND buffer_bench --self-test)

find_package(Qt6 6.5 QUIET COMPONENTS Widgets)
if(Qt6_FOUND)
    qt_standard_project_setup()
    qt_add_executable(editor_widget_prototype
        widget_prototype.cpp
        text_buffers.cpp
    )
    target_link_libraries(editor_widget_prototype PRIVATE Qt6::Widgets)
    target_compile_options(editor_widget_prototype PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-O2;-Wall;-Wextra;-Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/O2;/W4;/permissive->
    )
else()
    message(WARNING "Qt 6 Widgets not found; only the buffer benchmark target is available")
endif()
