cmake_minimum_required(VERSION 3.16)

project(fizmo
    VERSION 0.2.7
    DESCRIPTION "fizmo C++17 library"
    LANGUAGES CXX)

if(NOT (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    message(FATAL_ERROR "fizmo only supports Windows and Linux")
endif()

if(BUILD_SHARED_LIBS)
    set(FIZMO_SHARED_DEFAULT ON)
else()
    set(FIZMO_SHARED_DEFAULT OFF)
endif()
option(FIZMO_SHARED "Build fizmo as a shared library (.so / .dll) instead of a static one" ${FIZMO_SHARED_DEFAULT})

if(FIZMO_SHARED)
    add_library(fizmo SHARED)
else()
    add_library(fizmo STATIC)
endif()
add_library(fizmo::fizmo ALIAS fizmo)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoSources.cmake)
target_sources(fizmo PRIVATE ${FIZMO_SOURCES})

target_include_directories(fizmo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Includes>
    $<INSTALL_INTERFACE:include/fizmo/Includes>)

target_include_directories(fizmo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_compile_features(fizmo PUBLIC cxx_std_17)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoTuning.cmake)

set_target_properties(fizmo PROPERTIES
    OUTPUT_NAME fizmo
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON)

option(FIZMO_PCH "Use a precompiled header while building the fizmo library" ON)
if(FIZMO_PCH)
    target_precompile_headers(fizmo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmo_pch.hpp)
endif()

option(FIZMO_UNITY_BUILD "Compile the fizmo library sources in batches" OFF)
if(FIZMO_UNITY_BUILD)
    set_target_properties(fizmo PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 16)
endif()

if(MSVC)
    target_compile_options(fizmo PRIVATE /bigobj /utf-8)
elseif(MINGW)
    target_compile_options(fizmo PRIVATE -Wa,-mbig-obj)
    if(FIZMO_SHARED)
        target_link_options(fizmo PRIVATE -Wl,--exclude-libs,ALL)
    endif()
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoDependencies.cmake)
target_link_libraries(fizmo PRIVATE ${FIZMO_DEPENDENCY_TARGETS})

option(FIZMO_BUILD_SHADER_TOOL "Build fizmo-shaders, which turns GLSL into fizmo shader headers and bundles" ON)
if(FIZMO_BUILD_SHADER_TOOL)
    add_executable(fizmo-shaders tools/fizmo_shaders.cpp)
    add_executable(fizmo::shaders ALIAS fizmo-shaders)
    target_link_libraries(fizmo-shaders PRIVATE fizmo)
    target_compile_definitions(fizmo-shaders PRIVATE FIZMO_TOOL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(MINGW)
        target_compile_options(fizmo-shaders PRIVATE -Wa,-mbig-obj)
        target_link_options(fizmo-shaders PRIVATE -static)
    elseif(MSVC)
        target_compile_options(fizmo-shaders PRIVATE /bigobj /utf-8)
    endif()
    include(GNUInstallDirs)
    file(GENERATE OUTPUT "${FIZMO_GENERATED_DIR}/tools/$<CONFIG>/fizmo_tool_paths.hpp" CONTENT
"#ifndef FIZMO_TOOL_PATHS_HPP
#define FIZMO_TOOL_PATHS_HPP

#define FIZMO_SHADER_TOOL_BUILD_PATH \"$<TARGET_FILE:fizmo-shaders>\"
#define FIZMO_SHADER_TOOL_INSTALL_PATH \"${CMAKE_INSTALL_FULL_BINDIR}/fizmo-shaders${CMAKE_EXECUTABLE_SUFFIX}\"

#endif // FIZMO_TOOL_PATHS_HPP
")
    target_include_directories(fizmo PRIVATE "${FIZMO_GENERATED_DIR}/tools/$<CONFIG>")
endif()

option(FIZMO_BUILD_TOOLS "Build the fizmo command line tools" OFF)
if(FIZMO_BUILD_TOOLS)
    add_subdirectory(tools/mathtext)
endif()

option(FIZMO_BUILD_TEST "Build tests/test.cpp as fizmo-test" OFF)
if(FIZMO_BUILD_TEST)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.cpp)
        add_executable(fizmo-test tests/test.cpp)
    else()
        add_executable(fizmo-test test.cpp)
    endif()
    target_link_libraries(fizmo-test PRIVATE fizmo::fizmo)
    target_compile_definitions(fizmo-test PRIVATE
        FIZMO_MATH_EXAMPLE="${CMAKE_CURRENT_SOURCE_DIR}/examples/example.math"
        FIZMO_OUTPUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/output")
endif()

if(PROJECT_IS_TOP_LEVEL OR CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(FIZMO_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/fizmo)

    install(TARGETS fizmo EXPORT fizmoTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(FIZMO_BUILD_SHADER_TOOL)
        set_target_properties(fizmo-shaders PROPERTIES EXPORT_NAME shaders)
        install(TARGETS fizmo-shaders EXPORT fizmoTargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(DIRECTORY Includes/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fizmo/Includes)
    install(FILES ${FIZMO_GENERATED_DIR}/fizmo_mul_tuning.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fizmo/generated OPTIONAL)
    install(DIRECTORY Source/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fizmo/Source
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.inl")

    install(EXPORT fizmoTargets
        NAMESPACE fizmo::
        DESTINATION ${FIZMO_CMAKE_INSTALL_DIR})

    configure_package_config_file(
        cmake/fizmoConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/fizmoConfig.cmake
        INSTALL_DESTINATION ${FIZMO_CMAKE_INSTALL_DIR})

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/fizmoConfigVersion.cmake
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/fizmoConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/fizmoConfigVersion.cmake
        cmake/fizmoDependencies.cmake
        DESTINATION ${FIZMO_CMAKE_INSTALL_DIR})

    if(WIN32)
        if(MINGW)
            include(CheckCXXSourceCompiles)
            check_cxx_source_compiles("#include <cstdlib>\n#ifndef _UCRT\n#error msvcrt\n#endif\nint main() { return 0; }" FIZMO_MINGW_UCRT)
        endif()
        if(MINGW AND FIZMO_MINGW_UCRT)
            set(FIZMO_PACKAGE_PLATFORM_DEFAULT windows-ucrt64)
        elseif(MINGW)
            set(FIZMO_PACKAGE_PLATFORM_DEFAULT windows-mingw64)
        else()
            set(FIZMO_PACKAGE_PLATFORM_DEFAULT windows-msvc)
        endif()
        set(FIZMO_PACKAGE_GENERATOR_DEFAULT ZIP)
    else()
        set(FIZMO_PACKAGE_PLATFORM_DEFAULT linux-x86_64)
        set(FIZMO_PACKAGE_GENERATOR_DEFAULT TGZ)
    endif()
    set(FIZMO_PACKAGE_PLATFORM ${FIZMO_PACKAGE_PLATFORM_DEFAULT} CACHE STRING "Platform name used in the package file name")

    set(CPACK_PACKAGE_NAME fizmo)
    set(CPACK_PACKAGE_VENDOR ThatNewCoderJSCpp)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_FILE_NAME fizmo-${PROJECT_VERSION}-${FIZMO_PACKAGE_PLATFORM})
    set(CPACK_GENERATOR ${FIZMO_PACKAGE_GENERATOR_DEFAULT} CACHE STRING "Package formats cpack produces")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    set(CPACK_STRIP_FILES ON)
    include(CPack)
endif()
