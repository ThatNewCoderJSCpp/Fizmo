include(CMakeFindDependencyMacro)

if(FIZMO_BUNDLED_DEPS)
    include("${CMAKE_CURRENT_LIST_DIR}/fizmoBundledDeps.cmake")
endif()

if(NOT DEFINED FIZMO_DEPENDENCY_COMPONENTS)
    set(FIZMO_DEPENDENCY_COMPONENTS core net graphics)
endif()

set(FIZMO_CORE_DEPENDENCY_TARGETS "")
set(FIZMO_NET_DEPENDENCY_TARGETS "")
set(FIZMO_GRAPHICS_DEPENDENCY_TARGETS "")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_dependency(Threads)                      # -lpthread
list(APPEND FIZMO_CORE_DEPENDENCY_TARGETS Threads::Threads)

if(WIN32)
    list(APPEND FIZMO_CORE_DEPENDENCY_TARGETS
        gdi32 user32 kernel32 ole32 oleaut32 uuid comdlg32 shell32
        winmm wsock32 ws2_32 bcrypt secur32 crypt32 ncrypt dwrite msimg32
        iphlpapi advapi32)
elseif(ANDROID)
    list(APPEND FIZMO_CORE_DEPENDENCY_TARGETS m ${CMAKE_DL_LIBS} log android)
    if(FIZMO_BUNDLED_DEPS)
        list(APPEND FIZMO_CORE_DEPENDENCY_TARGETS $<BUILD_INTERFACE:fizmo_bundled_ssl> $<BUILD_INTERFACE:fizmo_bundled_crypto>)
    else()
        find_dependency(OpenSSL)
        list(APPEND FIZMO_CORE_DEPENDENCY_TARGETS OpenSSL::SSL OpenSSL::Crypto)
    endif()
elseif(APPLE)
    list(APPEND FIZMO_CORE_DEPENDENCY_TARGETS ${CMAKE_DL_LIBS}
        "-framework CoreFoundation" "-framework CoreServices" "-framework IOKit")
elseif(UNIX)
    list(APPEND FIZMO_CORE_DEPENDENCY_TARGETS m ${CMAKE_DL_LIBS})
endif()

if("graphics" IN_LIST FIZMO_DEPENDENCY_COMPONENTS)
    find_dependency(ZLIB)                     # -lz
    if(FIZMO_BUNDLED_DEPS)
        list(APPEND FIZMO_GRAPHICS_DEPENDENCY_TARGETS ZLIB::ZLIB $<BUILD_INTERFACE:fizmo_bundled_jpeg>)
    else()
        find_dependency(JPEG)                 # -ljpeg
        list(APPEND FIZMO_GRAPHICS_DEPENDENCY_TARGETS ZLIB::ZLIB JPEG::JPEG)
    endif()

    if(ANDROID)
        if(FIZMO_BUNDLED_DEPS)
            list(APPEND FIZMO_GRAPHICS_DEPENDENCY_TARGETS $<BUILD_INTERFACE:fizmo_bundled_harfbuzz> $<BUILD_INTERFACE:fizmo_bundled_freetype>)
        else()
            find_dependency(Freetype)
            find_dependency(PkgConfig)
            pkg_check_modules(FIZMO_HARFBUZZ REQUIRED IMPORTED_TARGET GLOBAL harfbuzz>=2.6)
            list(APPEND FIZMO_GRAPHICS_DEPENDENCY_TARGETS PkgConfig::FIZMO_HARFBUZZ Freetype::Freetype)
        endif()
        list(APPEND FIZMO_GRAPHICS_DEPENDENCY_TARGETS android log)
    elseif(APPLE)
        find_dependency(Fontconfig)           # -lfontconfig
        find_dependency(Freetype)             # -lfreetype

        find_dependency(PkgConfig)
        pkg_check_modules(FIZMO_HARFBUZZ REQUIRED IMPORTED_TARGET GLOBAL harfbuzz>=2.6)

        list(APPEND FIZMO_GRAPHICS_DEPENDENCY_TARGETS
            Fontconfig::Fontconfig Freetype::Freetype
            PkgConfig::FIZMO_HARFBUZZ
            "-framework Cocoa" "-framework QuartzCore" "-framework Metal"
            "-framework Carbon" "-framework UniformTypeIdentifiers"
            "-framework IOKit" "-framework CoreFoundation" "-framework CoreServices")
    elseif(UNIX AND NOT WIN32)
        find_dependency(X11)                  # -lX11
        if(NOT TARGET X11::Xft OR NOT TARGET X11::Xrender)
            message(FATAL_ERROR
                "fizmo: libXft / libXrender development files not found.\n"
                "  Fedora:        sudo dnf install libXft-devel libXrender-devel\n"
                "  Debian/Ubuntu: sudo apt install libxft-dev libxrender-dev")
        endif()
        find_dependency(Fontconfig)           # -lfontconfig
        find_dependency(Freetype)             # -lfreetype

        find_dependency(PkgConfig)
        pkg_check_modules(FIZMO_HARFBUZZ REQUIRED IMPORTED_TARGET GLOBAL harfbuzz>=2.6)

        list(APPEND FIZMO_GRAPHICS_DEPENDENCY_TARGETS
            X11::X11 X11::Xft X11::Xrender
            Fontconfig::Fontconfig Freetype::Freetype
            PkgConfig::FIZMO_HARFBUZZ)
    endif()
endif()

if("net" IN_LIST FIZMO_DEPENDENCY_COMPONENTS AND ANDROID)
    if(FIZMO_BUNDLED_DEPS)
        list(APPEND FIZMO_NET_DEPENDENCY_TARGETS $<BUILD_INTERFACE:fizmo_bundled_ssl> $<BUILD_INTERFACE:fizmo_bundled_crypto>)
    else()
        list(APPEND FIZMO_NET_DEPENDENCY_TARGETS OpenSSL::SSL OpenSSL::Crypto)
    endif()
elseif("net" IN_LIST FIZMO_DEPENDENCY_COMPONENTS AND UNIX AND NOT WIN32)
    find_dependency(OpenSSL)                  # -lssl -lcrypto
    list(APPEND FIZMO_NET_DEPENDENCY_TARGETS OpenSSL::SSL OpenSSL::Crypto)
endif()

set(FIZMO_DEPENDENCY_TARGETS
    ${FIZMO_CORE_DEPENDENCY_TARGETS}
    ${FIZMO_NET_DEPENDENCY_TARGETS}
    ${FIZMO_GRAPHICS_DEPENDENCY_TARGETS})
