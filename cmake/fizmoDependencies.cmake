include(CMakeFindDependencyMacro)

set(FIZMO_DEPENDENCY_TARGETS "")

find_dependency(ZLIB)                         # -lz
find_dependency(JPEG)                         # -ljpeg
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_dependency(Threads)                      # -lpthread

list(APPEND FIZMO_DEPENDENCY_TARGETS ZLIB::ZLIB JPEG::JPEG Threads::Threads)

if(WIN32)
    # Windows: system libraries that ship with every Windows / MinGW 
    # These don't need to be "found"; the linker already knows where they are
    list(APPEND FIZMO_DEPENDENCY_TARGETS
        gdi32 user32 kernel32 ole32 oleaut32 uuid comdlg32 shell32
        winmm wsock32 ws2_32 bcrypt secur32 crypt32 dwrite msimg32
        iphlpapi advapi32)

elseif(UNIX)
    find_dependency(X11)                      # -lX11 
    if(NOT TARGET X11::Xft OR NOT TARGET X11::Xrender)
        message(FATAL_ERROR
            "fizmo: libXft / libXrender development files not found.\n"
            "  Fedora:        sudo dnf install libXft-devel libXrender-devel\n"
            "  Debian/Ubuntu: sudo apt install libxft-dev libxrender-dev")
    endif()
    find_dependency(Fontconfig)               # -lfontconfig
    find_dependency(Freetype)                 # -lfreetype
    find_dependency(OpenSSL)                  # -lssl -lcrypto

    find_dependency(PkgConfig)
    pkg_check_modules(FIZMO_PANGOCAIRO REQUIRED IMPORTED_TARGET GLOBAL
        pangocairo pangoft2 pango cairo gobject-2.0 glib-2.0)

    list(APPEND FIZMO_DEPENDENCY_TARGETS
        X11::X11 X11::Xft X11::Xrender
        Fontconfig::Fontconfig Freetype::Freetype
        OpenSSL::SSL OpenSSL::Crypto
        PkgConfig::FIZMO_PANGOCAIRO
        m)                                    # -lm
endif()