cmake_minimum_required(VERSION 3.24)
project(Note VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
qt_standard_project_setup()

add_library(note_core
    src/editor/text_buffer.cpp
    src/documents/document.cpp
    src/documents/document_manager.cpp
    src/documents/file_service.cpp
)
target_include_directories(note_core PUBLIC src)
target_link_libraries(note_core PUBLIC Qt6::Core)

qt_add_executable(note
    src/main.cpp
)

target_link_libraries(note PRIVATE note_core Qt6::Widgets)

include(CTest)
if(BUILD_TESTING)
    add_executable(note_document_tests tests/documents/document_tests.cpp)
    target_link_libraries(note_document_tests PRIVATE note_core)
    add_test(NAME note_document_tests COMMAND note_document_tests)

    add_executable(note_document_bench
        benchmarks/document_foundation/document_bench.cpp
        benchmarks/editor_prototype/text_buffers.cpp
    )
    target_link_libraries(note_document_bench PRIVATE note_core)
    target_include_directories(note_document_bench PRIVATE benchmarks/editor_prototype)
endif()

if(MSVC)
    target_compile_options(note PRIVATE /W4 /permissive-)
    target_compile_options(note_core PRIVATE /W4 /permissive-)
    if(BUILD_TESTING)
        target_compile_options(note_document_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_document_bench PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(note PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(note_core PRIVATE -Wall -Wextra -Wpedantic $<$<CONFIG:Release>:-O2>)
    if(BUILD_TESTING)
        target_compile_options(note_document_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_document_bench PRIVATE -O2 -Wall -Wextra -Wpedantic)
    endif()
endif()
