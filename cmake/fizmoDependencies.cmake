include(CMakeFindDependencyMacro)

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
elseif(UNIX)
    list(APPEND FIZMO_CORE_DEPENDENCY_TARGETS m ${CMAKE_DL_LIBS})
endif()

if("graphics" IN_LIST FIZMO_DEPENDENCY_COMPONENTS)
    find_dependency(ZLIB)                     # -lz
    find_dependency(JPEG)                     # -ljpeg
    list(APPEND FIZMO_GRAPHICS_DEPENDENCY_TARGETS ZLIB::ZLIB JPEG::JPEG)

    if(UNIX AND NOT WIN32)
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

if("net" IN_LIST FIZMO_DEPENDENCY_COMPONENTS AND UNIX AND NOT WIN32)
    find_dependency(OpenSSL)                  # -lssl -lcrypto
    list(APPEND FIZMO_NET_DEPENDENCY_TARGETS OpenSSL::SSL OpenSSL::Crypto)
endif()

set(FIZMO_DEPENDENCY_TARGETS
    ${FIZMO_CORE_DEPENDENCY_TARGETS}
    ${FIZMO_NET_DEPENDENCY_TARGETS}
    ${FIZMO_GRAPHICS_DEPENDENCY_TARGETS})
