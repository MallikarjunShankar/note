# Install-time Windows deployment (run by `cmake --install`).
# 1. windeployqt copies the Qt libraries and the plugins Note needs.
# 2. Every DLL that note.exe or those plugins import from the toolchain's bin
#    directory (GCC runtime, Qt's third-party libraries) is copied beside them.
# System DLLs stay out of the package.

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW) # normalise paths before matching
endif()

set(app "${CMAKE_INSTALL_PREFIX}/note.exe")
if(NOT EXISTS "${app}")
    message(FATAL_ERROR "deploy: ${app} was not installed")
endif()

# Note uses no images, networking, TLS or touch input.
execute_process(
    COMMAND "${NOTE_WINDEPLOYQT}" --release --no-translations
            --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime
            --skip-plugin-types generic,networkinformation,tls,imageformats
            "${app}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "deploy: windeployqt failed (${result}):\n${output}")
endif()

file(GLOB_RECURSE modules "${CMAKE_INSTALL_PREFIX}/*.dll")
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${app}"
    LIBRARIES ${modules}
    DIRECTORIES "${NOTE_TOOLCHAIN_BIN}"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    # Qt DLLs exist both here (from windeployqt) and in the toolchain.
    CONFLICTING_DEPENDENCIES_PREFIX conflict
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-")
foreach(name IN LISTS conflict_FILENAMES)
    if(NOT EXISTS "${CMAKE_INSTALL_PREFIX}/${name}")
        message(FATAL_ERROR "deploy: ${name} found in several places: ${conflict_${name}}")
    endif()
endforeach()
if(unresolved)
    message(FATAL_ERROR "deploy: unresolved DLLs: ${unresolved}")
endif()

file(TO_CMAKE_PATH "${NOTE_TOOLCHAIN_BIN}" toolchain)
string(TOLOWER "${toolchain}/" toolchain)
set(copied 0)
foreach(dependency IN LISTS resolved)
    file(TO_CMAKE_PATH "${dependency}" path)
    string(TOLOWER "${path}" lower)
    string(FIND "${lower}" "${toolchain}" position)
    if(NOT position EQUAL 0)
        continue() # a system DLL
    endif()
    get_filename_component(name "${path}" NAME)
    if(NOT EXISTS "${CMAKE_INSTALL_PREFIX}/${name}")
        file(COPY "${path}" DESTINATION "${CMAKE_INSTALL_PREFIX}")
        math(EXPR copied "${copied} + 1")
    endif()
endforeach()
message(STATUS "deploy: windeployqt done; ${copied} toolchain DLLs added")
