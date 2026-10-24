#ifndef FIZMO_PLATFORM_INCLUDES_HPP
#define FIZMO_PLATFORM_INCLUDES_HPP

#include "fizmo_defines.hpp"
#include "platform_net_includes.hpp"

#if defined(OS_WINDOWS)
    #include <commctrl.h>
    #include <commdlg.h>
    #include <windowsx.h>

    #ifdef _MSC_VER
        #pragma comment(lib, "user32.lib")
        #pragma comment(lib, "gdi32.lib")
        #pragma comment(lib, "comdlg32.lib")
        #pragma comment(lib, "comctl32.lib")
        #pragma comment(lib, "shell32.lib")
    #endif
#elif defined(OS_LINUX)
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/Xatom.h>
    #include <X11/XKBlib.h>
    #include <X11/keysym.h>
    #include <X11/Xft/Xft.h>
    #include <fontconfig/fontconfig.h>
#endif

#endif // FIZMO_PLATFORM_INCLUDES_HPP
