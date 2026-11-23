cmake_minimum_required(VERSION 3.24)

# VERSION is the single source of the release version (SemVer, e.g. 1.0.0-rc.1).
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" NOTE_VERSION LIMIT_COUNT 1)
if(NOT NOTE_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-[0-9A-Za-z.]+)?$")
    message(FATAL_ERROR "VERSION must hold a version such as 1.0.0 or 1.0.0-rc.1, not '${NOTE_VERSION}'")
endif()
set(NOTE_VERSION_NUMERIC "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
set(NOTE_VERSION_PRERELEASE "${CMAKE_MATCH_4}")
project(Note VERSION ${NOTE_VERSION_NUMERIC} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
set(CMAKE_AUTOUIC OFF)
qt_standard_project_setup()

add_library(note_core
    src/editor/text_buffer.cpp
    src/editor/bracket_manager.cpp
    src/editor/cursor_manager.cpp
    src/editor/selection_manager.cpp
    src/editor/editing_engine.cpp
    src/editor/undo_redo_manager.cpp
    src/documents/document.cpp
    src/documents/document_manager.cpp
    src/documents/tab_manager.cpp
    src/documents/file_service.cpp
    src/platform/path_resolver.cpp
)
target_include_directories(note_core PUBLIC src)
target_link_libraries(note_core PUBLIC Qt6::Core)

add_library(note_app_core
    src/application/document_workflow.cpp
    src/application/session_recovery.cpp
    src/commands/command_registry.cpp
    src/commands/keybinding_manager.cpp
    src/commands/shortcut_dispatcher.cpp
    src/customisation/settings_manager.cpp
    src/customisation/theme_manager.cpp
    src/search/search_engine.cpp
    src/platform/file_monitor.cpp
    src/platform/launch_services.cpp
)
target_include_directories(note_app_core PUBLIC src)
target_link_libraries(note_app_core PUBLIC note_core Qt6::Gui)

qt_add_executable(note
    src/main.cpp
    src/ui/document_preview.cpp
    src/ui/document_preview.hpp
    src/ui/editor_renderer.cpp
    src/ui/editor_renderer.hpp
    src/ui/editor_view.cpp
    src/ui/editor_view.hpp
    src/ui/main_window.cpp
    src/ui/main_window.hpp
    src/ui/command_palette.cpp
    src/ui/command_palette.hpp
    src/ui/search_panel.cpp
    src/ui/search_panel.hpp
    src/ui/settings_dialog.cpp
    src/ui/settings_dialog.hpp
)

target_link_libraries(note PRIVATE note_app_core Qt6::Widgets)
# A GUI-subsystem program: no console window opens beside Note on Windows.
set_target_properties(note PROPERTIES WIN32_EXECUTABLE TRUE)
target_compile_definitions(note PRIVATE NOTE_VERSION="${NOTE_VERSION}")
configure_file(packaging/RELEASE_NOTES.md.in RELEASE_NOTES.md @ONLY)
if(WIN32)
    enable_language(RC)
    if(NOTE_VERSION_PRERELEASE)
        set(NOTE_RC_FILEFLAGS "VS_FF_PRERELEASE")
    else()
        set(NOTE_RC_FILEFLAGS "0x0L")
    endif()
    configure_file(packaging/note.rc.in note.rc @ONLY)
    target_sources(note PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/note.rc")
endif()

# `cmake --install` produces a self-contained folder: note.exe, notes and,
# on Windows, Qt plus every toolchain DLL it loads (cmake/deploy_windows.cmake).
install(TARGETS note RUNTIME DESTINATION .)
install(FILES LICENSE DESTINATION . RENAME LICENSE.txt)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/RELEASE_NOTES.md" DESTINATION .)
if(WIN32)
    get_filename_component(NOTE_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(NOTE_WINDEPLOYQT NAMES windeployqt6 windeployqt
                 HINTS "${NOTE_TOOLCHAIN_BIN}" REQUIRED)
    install(CODE "set(NOTE_WINDEPLOYQT \"${NOTE_WINDEPLOYQT}\")
                  set(NOTE_TOOLCHAIN_BIN \"${NOTE_TOOLCHAIN_BIN}\")")
    install(SCRIPT cmake/deploy_windows.cmake)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(note_cursor_tests tests/editor/cursor_manager_tests.cpp)
    target_link_libraries(note_cursor_tests PRIVATE note_core)
    add_test(NAME note_cursor_tests COMMAND note_cursor_tests)

    add_executable(note_bracket_tests tests/editor/bracket_manager_tests.cpp)
    target_link_libraries(note_bracket_tests PRIVATE note_core)
    add_test(NAME note_bracket_tests COMMAND note_bracket_tests)

    add_executable(note_selection_tests tests/editor/selection_manager_tests.cpp)
    target_link_libraries(note_selection_tests PRIVATE note_core)
    add_test(NAME note_selection_tests COMMAND note_selection_tests)

    add_executable(note_editing_tests tests/editor/editing_engine_tests.cpp)
    target_link_libraries(note_editing_tests PRIVATE note_core)
    add_test(NAME note_editing_tests COMMAND note_editing_tests)

    add_executable(note_undo_redo_tests tests/editor/undo_redo_manager_tests.cpp)
    target_link_libraries(note_undo_redo_tests PRIVATE note_core)
    add_test(NAME note_undo_redo_tests COMMAND note_undo_redo_tests)

    add_executable(note_command_tests tests/commands/command_system_tests.cpp)
    target_link_libraries(note_command_tests PRIVATE note_app_core Qt6::Widgets)
    add_test(NAME note_command_tests COMMAND note_command_tests)

    add_executable(note_customisation_tests tests/customisation/customisation_tests.cpp)
    target_link_libraries(note_customisation_tests PRIVATE note_app_core Qt6::Widgets)
    add_test(NAME note_customisation_tests COMMAND note_customisation_tests)

    add_executable(note_search_tests tests/search/search_tests.cpp)
    target_link_libraries(note_search_tests PRIVATE note_app_core)
    add_test(NAME note_search_tests COMMAND note_search_tests)

    add_executable(note_platform_services_tests
        tests/platform/platform_services_tests.cpp)
    target_link_libraries(note_platform_services_tests PRIVATE note_app_core)
    if(WIN32)
        add_test(NAME note_platform_services_tests
                 COMMAND note_platform_services_tests $<TARGET_FILE:note>)
    else()
        add_test(NAME note_platform_services_tests COMMAND note_platform_services_tests)
    endif()

    add_executable(note_ui_renderer_tests
        tests/ui/editor_renderer_tests.cpp
        src/ui/editor_renderer.cpp
        src/ui/editor_renderer.hpp
    )
    target_include_directories(note_ui_renderer_tests PRIVATE src)
    target_link_libraries(note_ui_renderer_tests PRIVATE note_core Qt6::Widgets)
    add_test(NAME note_ui_renderer_tests COMMAND note_ui_renderer_tests)
    set_tests_properties(note_ui_renderer_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(note_document_tests tests/documents/document_tests.cpp)
    target_link_libraries(note_document_tests PRIVATE note_core)
    add_test(NAME note_document_tests COMMAND note_document_tests)

    add_executable(note_document_workflow_tests tests/documents/document_workflow_tests.cpp)
    target_link_libraries(note_document_workflow_tests PRIVATE note_app_core)
    add_test(NAME note_document_workflow_tests COMMAND note_document_workflow_tests)

    add_executable(note_session_recovery_tests
        tests/application/session_recovery_tests.cpp)
    target_link_libraries(note_session_recovery_tests PRIVATE note_app_core)
    add_test(NAME note_session_recovery_tests COMMAND note_session_recovery_tests)

    add_executable(note_ui_smoke_tests
        tests/ui/main_window_smoke.cpp
        src/ui/document_preview.cpp
        src/ui/document_preview.hpp
        src/ui/editor_renderer.cpp
        src/ui/editor_renderer.hpp
        src/ui/editor_view.cpp
        src/ui/editor_view.hpp
        src/ui/main_window.cpp
        src/ui/main_window.hpp
        src/ui/command_palette.cpp
        src/ui/command_palette.hpp
        src/ui/search_panel.cpp
        src/ui/search_panel.hpp
        src/ui/settings_dialog.cpp
        src/ui/settings_dialog.hpp
    )
    target_include_directories(note_ui_smoke_tests PRIVATE src)
    target_link_libraries(note_ui_smoke_tests PRIVATE note_app_core Qt6::Widgets)
    add_test(NAME note_ui_smoke_tests COMMAND note_ui_smoke_tests)
    set_tests_properties(note_ui_smoke_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(note_session_lifecycle_tests
        tests/ui/session_lifecycle_smoke.cpp
        src/ui/document_preview.cpp
        src/ui/document_preview.hpp
        src/ui/editor_renderer.cpp
        src/ui/editor_renderer.hpp
        src/ui/editor_view.cpp
        src/ui/editor_view.hpp
        src/ui/main_window.cpp
        src/ui/main_window.hpp
        src/ui/command_palette.cpp
        src/ui/command_palette.hpp
        src/ui/search_panel.cpp
        src/ui/search_panel.hpp
        src/ui/settings_dialog.cpp
        src/ui/settings_dialog.hpp
    )
    target_include_directories(note_session_lifecycle_tests PRIVATE src)
    target_link_libraries(note_session_lifecycle_tests PRIVATE note_app_core Qt6::Widgets)
    add_test(NAME note_session_lifecycle_tests COMMAND note_session_lifecycle_tests)
    set_tests_properties(note_session_lifecycle_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(note_crash_recovery_tests
        tests/application/crash_recovery_tests.cpp
        src/ui/document_preview.cpp
        src/ui/editor_renderer.cpp
        src/ui/editor_view.cpp
        src/ui/main_window.cpp
        src/ui/command_palette.cpp
        src/ui/search_panel.cpp
        src/ui/settings_dialog.cpp
    )
    target_include_directories(note_crash_recovery_tests PRIVATE src)
    target_link_libraries(note_crash_recovery_tests PRIVATE note_app_core Qt6::Widgets)
    add_test(NAME note_crash_recovery_tests COMMAND note_crash_recovery_tests)
    set_tests_properties(note_crash_recovery_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)

    add_executable(note_document_bench
        benchmarks/document_foundation/document_bench.cpp
        benchmarks/editor_prototype/text_buffers.cpp
    )
    target_link_libraries(note_document_bench PRIVATE note_core)
    target_include_directories(note_document_bench PRIVATE benchmarks/editor_prototype)

    add_executable(note_async_open_bench benchmarks/asynchronous_open/async_open_bench.cpp)
    target_link_libraries(note_async_open_bench PRIVATE note_app_core Qt6::Core)
    if(WIN32)
        target_link_libraries(note_async_open_bench PRIVATE Psapi)
    endif()

    add_executable(note_editor_presentation_bench
        benchmarks/editor_presentation/editor_presentation_bench.cpp
        src/ui/editor_renderer.cpp
        src/ui/editor_renderer.hpp
    )
    target_include_directories(note_editor_presentation_bench PRIVATE src)
    target_link_libraries(note_editor_presentation_bench PRIVATE
        note_core Qt6::Widgets)

    add_executable(note_integrated_bench
        benchmarks/integrated/integrated_bench.cpp
        src/ui/document_preview.cpp
        src/ui/editor_renderer.cpp
        src/ui/editor_view.cpp
        src/ui/main_window.cpp
        src/ui/command_palette.cpp
        src/ui/search_panel.cpp
        src/ui/settings_dialog.cpp
    )
    target_include_directories(note_integrated_bench PRIVATE src)
    target_link_libraries(note_integrated_bench PRIVATE note_app_core Qt6::Widgets)
    if(WIN32)
        target_link_libraries(note_integrated_bench PRIVATE Psapi)
    endif()
endif()

if(MSVC)
    target_compile_options(note PRIVATE /W4 /permissive-)
    target_compile_options(note_core PRIVATE /W4 /permissive-)
    target_compile_options(note_app_core PRIVATE /W4 /permissive-)
    if(BUILD_TESTING)
        target_compile_options(note_cursor_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_bracket_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_selection_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_editing_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_undo_redo_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_ui_renderer_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_document_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_document_workflow_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_platform_services_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_ui_smoke_tests PRIVATE /W4 /permissive-)
        target_compile_options(note_document_bench PRIVATE /W4 /permissive-)
        target_compile_options(note_async_open_bench PRIVATE /W4 /permissive-)
        target_compile_options(note_editor_presentation_bench PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(note PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(note_core PRIVATE -Wall -Wextra -Wpedantic $<$<CONFIG:Release>:-O2>)
    target_compile_options(note_app_core PRIVATE -Wall -Wextra -Wpedantic)
    if(BUILD_TESTING)
        target_compile_options(note_cursor_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_bracket_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_selection_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_editing_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_undo_redo_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_ui_renderer_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_document_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_document_workflow_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_platform_services_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_ui_smoke_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(note_document_bench PRIVATE -O2 -Wall -Wextra -Wpedantic)
        target_compile_options(note_async_open_bench PRIVATE -O2 -Wall -Wextra -Wpedantic)
        target_compile_options(note_editor_presentation_bench PRIVATE -O2 -Wall -Wextra -Wpedantic)
    endif()
endif()
