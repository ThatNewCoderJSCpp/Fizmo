cmake_minimum_required(VERSION 3.16)

project(fizmo
    VERSION 0.2.12
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
option(FIZMO_SHARED "Make fizmo::fizmo the shared library (.so / .dll) instead of the static one" ${FIZMO_SHARED_DEFAULT})

if(PROJECT_IS_TOP_LEVEL OR CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(FIZMO_BUILD_STATIC_DEFAULT ON)
    set(FIZMO_BUILD_SHARED_DEFAULT ON)
elseif(FIZMO_SHARED)
    set(FIZMO_BUILD_STATIC_DEFAULT OFF)
    set(FIZMO_BUILD_SHARED_DEFAULT ON)
else()
    set(FIZMO_BUILD_STATIC_DEFAULT ON)
    set(FIZMO_BUILD_SHARED_DEFAULT OFF)
endif()
option(FIZMO_BUILD_STATIC "Build the static library (fizmo::fizmo_static)" ${FIZMO_BUILD_STATIC_DEFAULT})
option(FIZMO_BUILD_SHARED "Build the shared library (fizmo::fizmo_shared)" ${FIZMO_BUILD_SHARED_DEFAULT})

if(FIZMO_SHARED)
    set(FIZMO_BUILD_SHARED ON)
else()
    set(FIZMO_BUILD_STATIC ON)
endif()

add_library(fizmo_headers INTERFACE)
add_library(fizmo::headers ALIAS fizmo_headers)
set_target_properties(fizmo_headers PROPERTIES EXPORT_NAME headers)
target_include_directories(fizmo_headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Includes>
    $<INSTALL_INTERFACE:include/fizmo/Includes>)
target_compile_features(fizmo_headers INTERFACE cxx_std_17)
if(MSVC)
    target_compile_options(fizmo_headers INTERFACE $<$<COMPILE_LANGUAGE:CXX>:/bigobj /utf-8 /Zc:__cplusplus /permissive->)
elseif(MINGW)
    target_compile_options(fizmo_headers INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wa,-mbig-obj>)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    target_compile_options(fizmo_headers INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wno-psabi>)
endif()

add_library(fizmo_objects OBJECT)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoSources.cmake)
target_sources(fizmo_objects PRIVATE ${FIZMO_SOURCES})
target_include_directories(fizmo_objects PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_link_libraries(fizmo_objects PUBLIC fizmo_headers)
set_target_properties(fizmo_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoTuning.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoUnicode.cmake)

option(FIZMO_PCH "Use a precompiled header while building the fizmo library" ON)
if(FIZMO_PCH)
    target_precompile_headers(fizmo_objects PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmo_pch.hpp)
endif()

option(FIZMO_UNITY_BUILD "Compile the fizmo library sources in batches" OFF)
if(FIZMO_UNITY_BUILD)
    set_target_properties(fizmo_objects PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 16)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoDependencies.cmake)
target_link_libraries(fizmo_objects PRIVATE ${FIZMO_DEPENDENCY_TARGETS})

find_path(FIZMO_VULKAN_INCLUDE_DIR vulkan/vulkan.h)
if(NOT FIZMO_VULKAN_INCLUDE_DIR)
    message(FATAL_ERROR "fizmo: the Vulkan headers (vulkan/vulkan.h) were not found. Install the Vulkan headers package or set FIZMO_VULKAN_INCLUDE_DIR.")
endif()
target_include_directories(fizmo_objects SYSTEM PRIVATE ${FIZMO_VULKAN_INCLUDE_DIR})

if(FIZMO_SHARED)
    set(FIZMO_STATIC_TARGET fizmo_static)
    set(FIZMO_SHARED_TARGET fizmo)
else()
    set(FIZMO_STATIC_TARGET fizmo)
    set(FIZMO_SHARED_TARGET fizmo_shared)
endif()

function(fizmo_add_library target type)
    add_library(${target} ${type} $<TARGET_OBJECTS:fizmo_objects>)
    target_link_libraries(${target} PUBLIC fizmo_headers PRIVATE ${FIZMO_DEPENDENCY_TARGETS})
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Includes>
        $<BUILD_INTERFACE:${FIZMO_GENERATED_DIR}>
        $<INSTALL_INTERFACE:include/fizmo/Includes>
        $<INSTALL_INTERFACE:include/fizmo/generated>)
    target_compile_features(${target} PUBLIC cxx_std_17)
endfunction()

set(FIZMO_LIBRARY_TARGETS "")

if(FIZMO_BUILD_STATIC)
    fizmo_add_library(${FIZMO_STATIC_TARGET} STATIC)
    add_library(fizmo::fizmo_static ALIAS ${FIZMO_STATIC_TARGET})
    if(MSVC)
        set(_fizmo_static_name fizmo_static)
    else()
        set(_fizmo_static_name fizmo)
    endif()
    set_target_properties(${FIZMO_STATIC_TARGET} PROPERTIES
        OUTPUT_NAME ${_fizmo_static_name}
        EXPORT_NAME fizmo_static
        POSITION_INDEPENDENT_CODE ON)
    list(APPEND FIZMO_LIBRARY_TARGETS ${FIZMO_STATIC_TARGET})
endif()

if(FIZMO_BUILD_SHARED)
    fizmo_add_library(${FIZMO_SHARED_TARGET} SHARED)
    add_library(fizmo::fizmo_shared ALIAS ${FIZMO_SHARED_TARGET})
    set_target_properties(${FIZMO_SHARED_TARGET} PROPERTIES
        OUTPUT_NAME fizmo
        EXPORT_NAME fizmo_shared
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        WINDOWS_EXPORT_ALL_SYMBOLS ON)
    if(MINGW)
        target_link_options(${FIZMO_SHARED_TARGET} PRIVATE -Wl,--exclude-libs,ALL)
    endif()
    list(APPEND FIZMO_LIBRARY_TARGETS ${FIZMO_SHARED_TARGET})
endif()

add_library(fizmo::fizmo ALIAS fizmo)

option(FIZMO_BUILD_SHADER_TOOL "Build fizmo-shaders, which turns GLSL into fizmo shader headers and bundles" ON)
if(FIZMO_BUILD_SHADER_TOOL)
    add_executable(fizmo-shaders tools/fizmo_shaders.cpp)
    add_executable(fizmo::shaders ALIAS fizmo-shaders)
    if(FIZMO_BUILD_STATIC)
        target_link_libraries(fizmo-shaders PRIVATE ${FIZMO_STATIC_TARGET})
    else()
        target_link_libraries(fizmo-shaders PRIVATE ${FIZMO_SHARED_TARGET})
    endif()
    target_compile_definitions(fizmo-shaders PRIVATE FIZMO_TOOL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(MINGW)
        target_link_options(fizmo-shaders PRIVATE -static)
    endif()
    include(GNUInstallDirs)
    file(GENERATE OUTPUT "${FIZMO_GENERATED_DIR}/tools/$<CONFIG>/fizmo_tool_paths.hpp" CONTENT
"#ifndef FIZMO_TOOL_PATHS_HPP
#define FIZMO_TOOL_PATHS_HPP

#define FIZMO_SHADER_TOOL_BUILD_PATH \"$<TARGET_FILE:fizmo-shaders>\"
#define FIZMO_SHADER_TOOL_INSTALL_PATH \"${CMAKE_INSTALL_FULL_BINDIR}/fizmo-shaders${CMAKE_EXECUTABLE_SUFFIX}\"

#endif // FIZMO_TOOL_PATHS_HPP
")
    target_include_directories(fizmo_objects PRIVATE "${FIZMO_GENERATED_DIR}/tools/$<CONFIG>")
endif()

option(FIZMO_BUILD_TOOLS "Build the fizmo command line tools" OFF)
if(FIZMO_BUILD_TOOLS)
    add_subdirectory(tools/mathtext)
    add_executable(fizmo-unicode tools/unicode/fizmo_unicode.cpp)
    target_compile_features(fizmo-unicode PRIVATE cxx_std_17)
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

    install(TARGETS fizmo_headers ${FIZMO_LIBRARY_TARGETS} EXPORT fizmoTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(FIZMO_BUILD_SHADER_TOOL)
        set_target_properties(fizmo-shaders PROPERTIES EXPORT_NAME shaders)
        install(TARGETS fizmo-shaders EXPORT fizmoTargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(DIRECTORY Includes/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fizmo/Includes)
    install(DIRECTORY DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fizmo/generated)
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

    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _fizmo_cpu)
    if(_fizmo_cpu MATCHES "^(x86_64|amd64|x64)$")
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(FIZMO_PACKAGE_ARCH i686)
        else()
            set(FIZMO_PACKAGE_ARCH x86_64)
        endif()
    elseif(_fizmo_cpu MATCHES "^(aarch64|arm64)$")
        set(FIZMO_PACKAGE_ARCH arm64)
    elseif(_fizmo_cpu MATCHES "^(i[3-6]86|x86)$")
        set(FIZMO_PACKAGE_ARCH i686)
    else()
        set(FIZMO_PACKAGE_ARCH ${_fizmo_cpu})
    endif()

    if(WIN32)
        if(MINGW)
            include(CheckCXXSourceCompiles)
            check_cxx_source_compiles("#include <cstdlib>\n#ifndef _UCRT\n#error msvcrt\n#endif\nint main() { return 0; }" FIZMO_MINGW_UCRT)
        endif()
        if(MINGW AND FIZMO_PACKAGE_ARCH STREQUAL "arm64")
            set(FIZMO_PACKAGE_PLATFORM_DEFAULT windows-clangarm64)
        elseif(MINGW AND FIZMO_MINGW_UCRT)
            set(FIZMO_PACKAGE_PLATFORM_DEFAULT windows-ucrt64)
        elseif(MINGW)
            set(FIZMO_PACKAGE_PLATFORM_DEFAULT windows-mingw64)
        elseif(FIZMO_PACKAGE_ARCH STREQUAL "x86_64")
            set(FIZMO_PACKAGE_PLATFORM_DEFAULT windows-msvc-x64)
        else()
            set(FIZMO_PACKAGE_PLATFORM_DEFAULT windows-msvc-${FIZMO_PACKAGE_ARCH})
        endif()
        set(FIZMO_PACKAGE_GENERATOR_DEFAULT ZIP)
    else()
        set(FIZMO_PACKAGE_PLATFORM_DEFAULT linux-${FIZMO_PACKAGE_ARCH})
        set(FIZMO_PACKAGE_GENERATOR_DEFAULT TGZ)
    endif()
    set(FIZMO_PACKAGE_PLATFORM ${FIZMO_PACKAGE_PLATFORM_DEFAULT} CACHE STRING "Platform name used in the package file name")

    set(CPACK_PACKAGE_NAME fizmo)
    set(CPACK_PACKAGE_VENDOR ThatNewCoderJSCpp)
    set(CPACK_PACKAGE_CONTACT "ThatNewCoderJSCpp <https://github.com/ThatNewCoderJSCpp/Fizmo>")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ThatNewCoderJSCpp/Fizmo")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_PACKAGE_DESCRIPTION "fizmo C++17 library: headers, static and shared libraries, CMake package files and the fizmo-shaders tool.")
    set(CPACK_PACKAGE_FILE_NAME fizmo-${PROJECT_VERSION}-${FIZMO_PACKAGE_PLATFORM})
    set(CPACK_GENERATOR ${FIZMO_PACKAGE_GENERATOR_DEFAULT} CACHE STRING "Package formats cpack produces")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    set(CPACK_STRIP_FILES ON)
    set(CPACK_THREADS 0)
    set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoCPackOptions.cmake)

    set(CPACK_DEBIAN_PACKAGE_NAME fizmo-dev)
    set(CPACK_DEBIAN_FILE_NAME ${CPACK_PACKAGE_FILE_NAME}.deb)
    set(CPACK_DEBIAN_PACKAGE_SECTION libdevel)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libx11-dev, libxft-dev, libxrender-dev, libfontconfig-dev | libfontconfig1-dev, libfreetype-dev | libfreetype6-dev, libssl-dev, libpango1.0-dev, libcairo2-dev, zlib1g-dev, libjpeg-dev, pkg-config")

    set(CPACK_RPM_PACKAGE_NAME fizmo-devel)
    set(CPACK_RPM_FILE_NAME ${CPACK_PACKAGE_FILE_NAME}.rpm)
    set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
    set(CPACK_RPM_PACKAGE_REQUIRES
        "libX11-devel, libXft-devel, libXrender-devel, fontconfig-devel, freetype-devel, openssl-devel, pango-devel, cairo-devel, zlib-devel, libjpeg-turbo-devel, pkgconf")
    set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION /usr/include /usr/lib/cmake /usr/lib64/cmake)
    set(CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none")
    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoRpmPostInstall.sh)
    set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoRpmPostUninstall.sh)
    if(FIZMO_PACKAGE_ARCH STREQUAL "arm64")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE aarch64)
    elseif(FIZMO_PACKAGE_ARCH STREQUAL "i686")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE i686)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
    endif()

    set(CPACK_NSIS_PACKAGE_NAME "fizmo ${PROJECT_VERSION} (${FIZMO_PACKAGE_PLATFORM})")
    set(CPACK_NSIS_DISPLAY_NAME "fizmo ${PROJECT_VERSION} (${FIZMO_PACKAGE_PLATFORM})")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "fizmo-${PROJECT_VERSION}-${FIZMO_PACKAGE_PLATFORM}")
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "fizmo-${PROJECT_VERSION}-${FIZMO_PACKAGE_PLATFORM}")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/ThatNewCoderJSCpp/Fizmo")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    if(FIZMO_PACKAGE_ARCH STREQUAL "x86_64" OR FIZMO_PACKAGE_ARCH STREQUAL "arm64")
        set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    endif()

    include(CPack)
endif()
