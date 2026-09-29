cmake_minimum_required(VERSION 3.16)

project(fizmo
    VERSION 0.1.8
    DESCRIPTION "fizmo C++17 library"
    LANGUAGES CXX)

if(NOT (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    message(FATAL_ERROR "fizmo only supports Windows and Linux")
endif()


add_library(fizmo INTERFACE)
add_library(fizmo::fizmo ALIAS fizmo)  

target_include_directories(fizmo INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Includes>
    $<INSTALL_INTERFACE:include/Fizmo>)

target_compile_features(fizmo INTERFACE cxx_std_17)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fizmoDependencies.cmake)
target_link_libraries(fizmo INTERFACE ${FIZMO_DEPENDENCY_TARGETS})

if(PROJECT_IS_TOP_LEVEL OR CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(FIZMO_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/fizmo)

    install(TARGETS fizmo EXPORT fizmoTargets)
    install(DIRECTORY Includes/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Fizmo)

    install(EXPORT fizmoTargets
        NAMESPACE fizmo::
        DESTINATION ${FIZMO_CMAKE_INSTALL_DIR})

    configure_package_config_file(
        cmake/fizmoConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/fizmoConfig.cmake
        INSTALL_DESTINATION ${FIZMO_CMAKE_INSTALL_DIR})

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/fizmoConfigVersion.cmake
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/fizmoConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/fizmoConfigVersion.cmake
        cmake/fizmoDependencies.cmake
        DESTINATION ${FIZMO_CMAKE_INSTALL_DIR})
endif()