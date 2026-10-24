cmake_minimum_required(VERSION 3.16)

project(fizmo
    VERSION 0.3.4
    DESCRIPTION "fizmo C++17 library"
    LANGUAGES CXX)

if(NOT (WIN32 OR APPLE OR ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    message(FATAL_ERROR "fizmo only supports Windows, Linux, macOS and Android")
endif()

if(ANDROID)
    set(FIZMO_BUNDLED_DEPS_DEFAULT ON)
else()
    set(FIZMO_BUNDLED_DEPS_DEFAULT OFF)
endif()
option(FIZMO_BUNDLED_DEPS "Build FreeType, HarfBuzz and libjpeg-turbo (and OpenSSL on Android) from source instead of using the installed ones" ${FIZMO_BUNDLED_DEPS_DEFAULT})

if(APPLE)
    enable_language(OBJCXX)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Oldest macOS version fizmo builds for" FORCE)
    endif()
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
if(MSVC AND NOT FIZMO_SHARED)
    set(FIZMO_BUILD_STATIC_DEFAULT ON)
    set(FIZMO_BUILD_SHARED_DEFAULT OFF)
endif()
option(FIZMO_BUILD_STATIC "Build the static library (fizmo::fizmo_static)" ${FIZMO_BUILD_STATIC_DEFAULT})
option(FIZMO_BUILD_SHARED "Build the shared library (fizmo::fizmo_shared)" ${FIZMO_BUILD_SHARED_DEFAULT})

if(FIZMO_SHARED)
    set(FIZMO_BUILD_SHARED ON)
    if(MSVC)
        message(WARNING "fizmo exports more than 65535 symbols, which an MSVC DLL import library cannot hold; linking fizmo.dll is expected to fail with LNK1189")
    endif()
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

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoSources.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoTuning.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoUnicode.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoDependencies.cmake)

if(FIZMO_BUNDLED_DEPS)
    target_include_directories(fizmo_headers SYSTEM INTERFACE
        $<BUILD_INTERFACE:${FIZMO_BUNDLED_PREFIX}/include>
        $<BUILD_INTERFACE:${FIZMO_BUNDLED_PREFIX}/include/freetype2>
        $<BUILD_INTERFACE:${FIZMO_BUNDLED_PREFIX}/include/harfbuzz>)
    foreach(_fizmo_bundled IN ITEMS fizmo_bundled_jpeg fizmo_bundled_freetype fizmo_bundled_harfbuzz fizmo_bundled_ssl)
        if(TARGET ${_fizmo_bundled})
            add_dependencies(fizmo_headers ${_fizmo_bundled})
        endif()
    endforeach()
    unset(_fizmo_bundled)
endif()

option(FIZMO_PCH "Use a precompiled header while building the fizmo library" ON)
option(FIZMO_UNITY_BUILD "Compile the fizmo library sources in batches" OFF)

if(APPLE)
    find_path(FIZMO_VULKAN_INCLUDE_DIR vulkan/vulkan.h HINTS /opt/homebrew/opt/vulkan-headers/include /usr/local/opt/vulkan-headers/include /opt/homebrew/include /usr/local/include)
else()
    find_path(FIZMO_VULKAN_INCLUDE_DIR vulkan/vulkan.h)
endif()
if(NOT FIZMO_VULKAN_INCLUDE_DIR)
    message(FATAL_ERROR "fizmo: the Vulkan headers (vulkan/vulkan.h) were not found. Install the Vulkan headers package or set FIZMO_VULKAN_INCLUDE_DIR.")
endif()

function(fizmo_add_objects target)
    add_library(${target} OBJECT ${ARGN})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_include_directories(${target} SYSTEM PRIVATE ${FIZMO_VULKAN_INCLUDE_DIR})
    target_link_libraries(${target} PUBLIC fizmo_headers PRIVATE ${FIZMO_DEPENDENCY_TARGETS})
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(FIZMO_UNITY_BUILD)
        set_target_properties(${target} PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 16)
    endif()
endfunction()

set(FIZMO_ALL_OBJECTS "")

foreach(_fizmo_component IN LISTS FIZMO_COMPONENTS)
    fizmo_add_objects(fizmo_${_fizmo_component}_objects ${FIZMO_COMPONENT_SOURCES_${_fizmo_component}})
    list(APPEND FIZMO_ALL_OBJECTS $<TARGET_OBJECTS:fizmo_${_fizmo_component}_objects>)

    if(FIZMO_PCH AND _fizmo_component STREQUAL "core")
        target_precompile_headers(fizmo_core_objects PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmo_pch.hpp)
    elseif(FIZMO_PCH)
        target_precompile_headers(fizmo_${_fizmo_component}_objects REUSE_FROM fizmo_core_objects)
    endif()
endforeach()

if(FIZMO_SHARED)
    set(FIZMO_STATIC_TARGET fizmo_static)
    set(FIZMO_SHARED_TARGET fizmo)
else()
    set(FIZMO_STATIC_TARGET fizmo)
    set(FIZMO_SHARED_TARGET fizmo_shared)
endif()

function(fizmo_add_library target type)
    add_library(${target} ${type} ${FIZMO_ALL_OBJECTS})
    target_link_libraries(${target} PUBLIC fizmo_headers PRIVATE ${FIZMO_DEPENDENCY_TARGETS})
    fizmo_add_interface_dirs(${target})
    if(ANDROID AND type STREQUAL "STATIC")
        target_link_options(${target} INTERFACE "LINKER:-u,ANativeActivity_onCreate")
    endif()
endfunction()

function(fizmo_add_interface_dirs target)
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
        include(CheckCXXSourceCompiles)
        set(_fizmo_saved_link_options "${CMAKE_REQUIRED_LINK_OPTIONS}")
        set(CMAKE_REQUIRED_LINK_OPTIONS "-Wl,--exclude-libs,ALL")
        check_cxx_source_compiles("int main() { return 0; }" FIZMO_LINKER_HAS_EXCLUDE_LIBS)
        set(CMAKE_REQUIRED_LINK_OPTIONS "${_fizmo_saved_link_options}")
        if(FIZMO_LINKER_HAS_EXCLUDE_LIBS)
            target_link_options(${FIZMO_SHARED_TARGET} PRIVATE -Wl,--exclude-libs,ALL)
        endif()
    endif()
    list(APPEND FIZMO_LIBRARY_TARGETS ${FIZMO_SHARED_TARGET})
endif()

add_library(fizmo::fizmo ALIAS fizmo)

option(FIZMO_BUILD_COMPONENTS "Build each fizmo component (fizmo::core, fizmo::net, fizmo::graphics, ...) as a static library that links only what it uses" ON)
set(FIZMO_COMPONENT_TARGETS "")

if(FIZMO_BUILD_COMPONENTS)
    foreach(_fizmo_component IN LISTS FIZMO_COMPONENTS)
        add_library(fizmo_${_fizmo_component} STATIC $<TARGET_OBJECTS:fizmo_${_fizmo_component}_objects>)

        if(_fizmo_component STREQUAL "core")
            target_link_libraries(fizmo_core PUBLIC fizmo_headers PRIVATE ${FIZMO_CORE_DEPENDENCY_TARGETS})
        else()
            set(_fizmo_uses "")
            foreach(_fizmo_use IN LISTS FIZMO_COMPONENT_USES_${_fizmo_component})
                list(APPEND _fizmo_uses fizmo_${_fizmo_use})
            endforeach()
            target_link_libraries(fizmo_${_fizmo_component} PUBLIC ${_fizmo_uses})
        endif()

        if(_fizmo_component STREQUAL "net")
            target_link_libraries(fizmo_net PUBLIC ${FIZMO_NET_DEPENDENCY_TARGETS})
        elseif(_fizmo_component STREQUAL "graphics")
            target_link_libraries(fizmo_graphics PRIVATE ${FIZMO_GRAPHICS_DEPENDENCY_TARGETS})
            if(ANDROID)
                target_link_options(fizmo_graphics INTERFACE "LINKER:-u,ANativeActivity_onCreate")
            endif()
        endif()
    endforeach()

    foreach(_fizmo_component IN LISTS FIZMO_COMPONENTS)
        fizmo_add_interface_dirs(fizmo_${_fizmo_component})
        set_target_properties(fizmo_${_fizmo_component} PROPERTIES
            OUTPUT_NAME fizmo_${_fizmo_component}
            EXPORT_NAME ${_fizmo_component}
            POSITION_INDEPENDENT_CODE ON)
        add_library(fizmo::${_fizmo_component} ALIAS fizmo_${_fizmo_component})
        list(APPEND FIZMO_COMPONENT_TARGETS fizmo_${_fizmo_component})
    endforeach()
    unset(_fizmo_component)
    unset(_fizmo_use)
    unset(_fizmo_uses)
endif()

if(CMAKE_CROSSCOMPILING)
    set(FIZMO_BUILD_SHADER_TOOL_DEFAULT OFF)
else()
    set(FIZMO_BUILD_SHADER_TOOL_DEFAULT ON)
endif()
option(FIZMO_BUILD_SHADER_TOOL "Build fizmo-shaders, which turns GLSL into fizmo shader headers and bundles" ${FIZMO_BUILD_SHADER_TOOL_DEFAULT})
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
    target_include_directories(fizmo_graphics_objects PRIVATE "${FIZMO_GENERATED_DIR}/tools/$<CONFIG>")
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

option(FIZMO_BUILD_NUMERICS_TESTS "Build Testing/numerics_tests.cpp as fizmo-numerics-tests" OFF)
if(FIZMO_BUILD_NUMERICS_TESTS)
    add_executable(fizmo-numerics-tests Testing/numerics_tests.cpp)
    if(TARGET fizmo::numerics)
        target_link_libraries(fizmo-numerics-tests PRIVATE fizmo::numerics)
    else()
        target_link_libraries(fizmo-numerics-tests PRIVATE fizmo::fizmo)
    endif()
    enable_testing()
    add_test(NAME fizmo-numerics COMMAND fizmo-numerics-tests)
endif()

option(FIZMO_BUILD_GUARD_TESTS "Build Testing/guard_tests.cpp as fizmo-guard-tests" OFF)
if(FIZMO_BUILD_GUARD_TESTS)
    add_executable(fizmo-guard-tests Testing/guard_tests.cpp)
    target_compile_features(fizmo-guard-tests PRIVATE cxx_std_17)
    if(TARGET fizmo::net)
        target_link_libraries(fizmo-guard-tests PRIVATE fizmo::net)
    else()
        target_link_libraries(fizmo-guard-tests PRIVATE fizmo::fizmo)
    endif()
    enable_testing()
    add_test(NAME fizmo-guard COMMAND fizmo-guard-tests)
endif()

option(FIZMO_BUILD_DATABASE_TESTS "Build Testing/database_tests.cpp as fizmo-database-tests" OFF)
if(FIZMO_BUILD_DATABASE_TESTS)
    add_executable(fizmo-database-tests Testing/database_tests.cpp)
    target_compile_features(fizmo-database-tests PRIVATE cxx_std_17)
    if(TARGET fizmo::db)
        target_link_libraries(fizmo-database-tests PRIVATE fizmo::db)
    else()
        target_link_libraries(fizmo-database-tests PRIVATE fizmo::fizmo)
    endif()
    enable_testing()
    add_test(NAME fizmo-database COMMAND fizmo-database-tests)
endif()

if(PROJECT_IS_TOP_LEVEL OR CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(FIZMO_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/fizmo)

    install(TARGETS fizmo_headers ${FIZMO_LIBRARY_TARGETS} ${FIZMO_COMPONENT_TARGETS} EXPORT fizmoTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(FIZMO_BUNDLED_DEPS)
        install(DIRECTORY "${FIZMO_BUNDLED_PREFIX}/lib/" DESTINATION ${CMAKE_INSTALL_LIBDIR}
            FILES_MATCHING PATTERN "*.a" PATTERN "cmake" EXCLUDE PATTERN "pkgconfig" EXCLUDE)
    endif()
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
        elseif(MINGW AND FIZMO_PACKAGE_ARCH STREQUAL "i686")
            set(FIZMO_PACKAGE_PLATFORM_DEFAULT windows-i686)
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
    elseif(ANDROID)
        set(FIZMO_PACKAGE_PLATFORM_DEFAULT android-${ANDROID_ABI})
        set(FIZMO_PACKAGE_GENERATOR_DEFAULT TGZ)
    elseif(APPLE)
        set(FIZMO_PACKAGE_PLATFORM_DEFAULT macos-${FIZMO_PACKAGE_ARCH})
        set(FIZMO_PACKAGE_GENERATOR_DEFAULT TGZ)
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
        "libx11-dev, libxft-dev, libxrender-dev, libfontconfig-dev | libfontconfig1-dev, libfreetype-dev | libfreetype6-dev, libssl-dev, libharfbuzz-dev, zlib1g-dev, libjpeg-dev, pkg-config")

    set(CPACK_RPM_PACKAGE_NAME fizmo-devel)
    set(CPACK_RPM_FILE_NAME ${CPACK_PACKAGE_FILE_NAME}.rpm)
    set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
    set(CPACK_RPM_PACKAGE_REQUIRES
        "libX11-devel, libXft-devel, libXrender-devel, fontconfig-devel, freetype-devel, openssl-devel, harfbuzz-devel, zlib-devel, libjpeg-turbo-devel, pkgconf")
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
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/Branding/fizmo.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/Branding/fizmo.ico")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    if(FIZMO_PACKAGE_ARCH STREQUAL "x86_64" OR FIZMO_PACKAGE_ARCH STREQUAL "arm64")
        set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    endif()

    include(CPack)
endif()
