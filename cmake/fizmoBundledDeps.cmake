if(TARGET fizmo_bundled_jpeg)
    return()
endif()

if(CMAKE_VERSION VERSION_LESS 3.19)
    message(FATAL_ERROR "fizmo: FIZMO_BUNDLED_DEPS needs CMake 3.19 or newer")
endif()

include(ExternalProject)

set(FIZMO_BUNDLED_PREFIX "${CMAKE_BINARY_DIR}/fizmo-bundled" CACHE INTERNAL "Where fizmo installs the dependencies it builds from source")
set(FIZMO_BUNDLED_DOWNLOADS "${CMAKE_BINARY_DIR}/fizmo-bundled-downloads" CACHE PATH "Where fizmo keeps the source archives of the dependencies it builds")
file(MAKE_DIRECTORY "${FIZMO_BUNDLED_PREFIX}/include" "${FIZMO_BUNDLED_PREFIX}/include/freetype2" "${FIZMO_BUNDLED_PREFIX}/include/harfbuzz" "${FIZMO_BUNDLED_PREFIX}/lib")

set(FIZMO_BUNDLED_JPEG_VERSION "3.1.2")
set(FIZMO_BUNDLED_JPEG_SHA256 "8f0012234b464ce50890c490f18194f913a7b1f4e6a03d6644179fa0f867d0cf")
set(FIZMO_BUNDLED_FREETYPE_TAG "VER-2-13-3")
set(FIZMO_BUNDLED_HARFBUZZ_VERSION "11.5.1")
set(FIZMO_BUNDLED_HARFBUZZ_SHA256 "972a60a8d274d49e70361da6920c3a73dfb0fb4387f6c6811906a47ba634d8a1")
set(FIZMO_BUNDLED_OPENSSL_VERSION "3.5.4")
set(FIZMO_BUNDLED_OPENSSL_SHA256 "967311f84955316969bdb1d8d4b983718ef42338639c621ec4c34fddef355e99")

set(_fizmo_bundled_args
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=${FIZMO_BUNDLED_PREFIX}
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_INSTALL_INCLUDEDIR=include
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5)
if(CMAKE_GENERATOR)
    list(APPEND _fizmo_bundled_args "-G${CMAKE_GENERATOR}")
endif()
if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND _fizmo_bundled_args -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()
if(CMAKE_MAKE_PROGRAM)
    list(APPEND _fizmo_bundled_args -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM})
endif()
if(ANDROID)
    list(APPEND _fizmo_bundled_args
        -DANDROID_ABI=${ANDROID_ABI}
        -DANDROID_PLATFORM=${ANDROID_PLATFORM}
        -DANDROID_STL=${ANDROID_STL})
    if(DEFINED ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES)
        list(APPEND _fizmo_bundled_args -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=${ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES})
    endif()
endif()
if(APPLE)
    list(APPEND _fizmo_bundled_args
        -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}")
endif()

set(_fizmo_lib "${FIZMO_BUNDLED_PREFIX}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}")
set(_fizmo_a "${CMAKE_STATIC_LIBRARY_SUFFIX}")

ExternalProject_Add(fizmo_bundled_jpeg_build
    URL "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${FIZMO_BUNDLED_JPEG_VERSION}/libjpeg-turbo-${FIZMO_BUNDLED_JPEG_VERSION}.tar.gz"
    URL_HASH SHA256=${FIZMO_BUNDLED_JPEG_SHA256}
    DOWNLOAD_DIR "${FIZMO_BUNDLED_DOWNLOADS}"
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    CMAKE_ARGS ${_fizmo_bundled_args}
        -DENABLE_SHARED=OFF
        -DENABLE_STATIC=ON
        -DWITH_TURBOJPEG=OFF
        -DWITH_TOOLS=OFF
        -DWITH_TESTS=OFF
        -DREQUIRE_SIMD=OFF
    BUILD_BYPRODUCTS "${_fizmo_lib}jpeg${_fizmo_a}"
    UPDATE_COMMAND ""
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    LOG_OUTPUT_ON_FAILURE ON)

ExternalProject_Add(fizmo_bundled_freetype_build
    GIT_REPOSITORY "https://github.com/freetype/freetype.git"
    GIT_TAG ${FIZMO_BUNDLED_FREETYPE_TAG}
    GIT_SHALLOW ON
    GIT_PROGRESS OFF
    CMAKE_ARGS ${_fizmo_bundled_args}
        -DFT_DISABLE_ZLIB=ON
        -DFT_DISABLE_BZIP2=ON
        -DFT_DISABLE_PNG=ON
        -DFT_DISABLE_HARFBUZZ=ON
        -DFT_DISABLE_BROTLI=ON
    BUILD_BYPRODUCTS "${_fizmo_lib}freetype${_fizmo_a}"
    UPDATE_COMMAND ""
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    LOG_OUTPUT_ON_FAILURE ON)

ExternalProject_Add(fizmo_bundled_harfbuzz_build
    URL "https://github.com/harfbuzz/harfbuzz/releases/download/${FIZMO_BUNDLED_HARFBUZZ_VERSION}/harfbuzz-${FIZMO_BUNDLED_HARFBUZZ_VERSION}.tar.xz"
    URL_HASH SHA256=${FIZMO_BUNDLED_HARFBUZZ_SHA256}
    DOWNLOAD_DIR "${FIZMO_BUNDLED_DOWNLOADS}"
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    CMAKE_ARGS ${_fizmo_bundled_args}
        -DHB_HAVE_FREETYPE=OFF
        -DHB_HAVE_GLIB=OFF
        -DHB_HAVE_ICU=OFF
        -DHB_HAVE_GOBJECT=OFF
        -DHB_HAVE_INTROSPECTION=OFF
        -DHB_BUILD_UTILS=OFF
        -DHB_BUILD_SUBSET=OFF
        -DHB_BUILD_TESTS=OFF
    BUILD_BYPRODUCTS "${_fizmo_lib}harfbuzz${_fizmo_a}"
    UPDATE_COMMAND ""
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    LOG_OUTPUT_ON_FAILURE ON)

function(fizmo_bundled_library name build libs includes)
    add_library(${name} INTERFACE)
    target_include_directories(${name} SYSTEM INTERFACE ${includes})
    target_link_libraries(${name} INTERFACE ${libs})
    add_dependencies(${name} ${build})
endfunction()

fizmo_bundled_library(fizmo_bundled_jpeg fizmo_bundled_jpeg_build
    "${_fizmo_lib}jpeg${_fizmo_a}" "${FIZMO_BUNDLED_PREFIX}/include")
fizmo_bundled_library(fizmo_bundled_freetype fizmo_bundled_freetype_build
    "${_fizmo_lib}freetype${_fizmo_a}" "${FIZMO_BUNDLED_PREFIX}/include/freetype2")
fizmo_bundled_library(fizmo_bundled_harfbuzz fizmo_bundled_harfbuzz_build
    "${_fizmo_lib}harfbuzz${_fizmo_a}" "${FIZMO_BUNDLED_PREFIX}/include/harfbuzz")

if(ANDROID)
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(_fizmo_openssl_target android-arm64)
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(_fizmo_openssl_target android-arm)
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(_fizmo_openssl_target android-x86_64)
    elseif(ANDROID_ABI STREQUAL "x86")
        set(_fizmo_openssl_target android-x86)
    else()
        message(FATAL_ERROR "fizmo: no OpenSSL target for the Android ABI ${ANDROID_ABI}")
    endif()

    string(REGEX REPLACE "[^0-9]" "" _fizmo_android_api "${ANDROID_PLATFORM}")
    if(NOT _fizmo_android_api)
        set(_fizmo_android_api ${CMAKE_SYSTEM_VERSION})
    endif()

    if(ANDROID_TOOLCHAIN_ROOT)
        set(_fizmo_ndk_bin "${ANDROID_TOOLCHAIN_ROOT}/bin")
    else()
        set(_fizmo_ndk_bin "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${CMAKE_ANDROID_NDK_TOOLCHAIN_HOST_TAG}/bin")
    endif()
    if(ANDROID_NDK)
        set(_fizmo_ndk_root "${ANDROID_NDK}")
    else()
        set(_fizmo_ndk_root "${CMAKE_ANDROID_NDK}")
    endif()

    find_program(FIZMO_PERL perl REQUIRED)
    find_program(FIZMO_MAKE NAMES gmake make REQUIRED)
    include(ProcessorCount)
    ProcessorCount(_fizmo_jobs)
    if(_fizmo_jobs EQUAL 0)
        set(_fizmo_jobs 2)
    endif()

    if(CMAKE_HOST_WIN32)
        set(_fizmo_path "${_fizmo_ndk_bin};$ENV{PATH}")
        string(REPLACE ";" "$<SEMICOLON>" _fizmo_path "${_fizmo_path}")
    else()
        set(_fizmo_path "${_fizmo_ndk_bin}:$ENV{PATH}")
    endif()
    set(_fizmo_openssl_env ${CMAKE_COMMAND} -E env
        "ANDROID_NDK_ROOT=${_fizmo_ndk_root}"
        "ANDROID_NDK_HOME=${_fizmo_ndk_root}"
        "PATH=${_fizmo_path}")

    ExternalProject_Add(fizmo_bundled_openssl_build
        URL "https://github.com/openssl/openssl/releases/download/openssl-${FIZMO_BUNDLED_OPENSSL_VERSION}/openssl-${FIZMO_BUNDLED_OPENSSL_VERSION}.tar.gz"
        URL_HASH SHA256=${FIZMO_BUNDLED_OPENSSL_SHA256}
        DOWNLOAD_DIR "${FIZMO_BUNDLED_DOWNLOADS}"
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        BUILD_IN_SOURCE ON
        CONFIGURE_COMMAND ${_fizmo_openssl_env} ${FIZMO_PERL} Configure ${_fizmo_openssl_target}
            -D__ANDROID_API__=${_fizmo_android_api}
            no-shared no-tests no-docs no-apps no-module
            --prefix=${FIZMO_BUNDLED_PREFIX} --libdir=lib --openssldir=/system/etc/security
        BUILD_COMMAND ${_fizmo_openssl_env} ${FIZMO_MAKE} -j${_fizmo_jobs} build_libs
        INSTALL_COMMAND ${_fizmo_openssl_env} ${FIZMO_MAKE} install_dev
        BUILD_BYPRODUCTS "${_fizmo_lib}ssl${_fizmo_a}" "${_fizmo_lib}crypto${_fizmo_a}"
        UPDATE_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
        LOG_OUTPUT_ON_FAILURE ON)

    fizmo_bundled_library(fizmo_bundled_crypto fizmo_bundled_openssl_build
        "${_fizmo_lib}crypto${_fizmo_a}" "${FIZMO_BUNDLED_PREFIX}/include")
    fizmo_bundled_library(fizmo_bundled_ssl fizmo_bundled_openssl_build
        "${_fizmo_lib}ssl${_fizmo_a};fizmo_bundled_crypto" "${FIZMO_BUNDLED_PREFIX}/include")
endif()

unset(_fizmo_bundled_args)
unset(_fizmo_lib)
unset(_fizmo_a)
