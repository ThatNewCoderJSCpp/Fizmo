#ifndef FIZMO_PLATFORM_INCLUDES_HPP
#define FIZMO_PLATFORM_INCLUDES_HPP

#include "fizmo_defines.hpp"

#if defined(OS_WINDOWS)
    #include <winsock2.h>
    #include <windows.h>
    #include <Windows.h>
    #include <CommCtrl.h>
    #include <commdlg.h>
    #include <windowsx.h>
    #include <wincrypt.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>
    #include <mstcpip.h>
    #include <iphlpapi.h>
    #include <security.h>
    #include <schannel.h>

    #ifdef _MSC_VER
        #pragma comment(lib, "user32.lib")
        #pragma comment(lib, "gdi32.lib")
        #pragma comment(lib, "comdlg32.lib")
        #pragma comment(lib, "comctl32.lib")
        #pragma comment(lib, "ws2_32.lib")
        #pragma comment(lib, "crypt32.lib")
        #pragma comment(lib, "secur32.lib")
        #pragma comment(lib, "iphlpapi.lib")
        #pragma comment(lib, "advapi32.lib")
        #pragma comment(lib, "shell32.lib")
    #endif
#elif defined(OS_LINUX)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/uio.h>
    #include <sys/ioctl.h>
    #include <sys/epoll.h>
    #include <sys/eventfd.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <net/if.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <errno.h>
    #include <string.h>
    #include <signal.h>

    #include <time.h>
    #include <sched.h>
    #include <sys/prctl.h>

    #include <openssl/ssl.h>
    #include <openssl/err.h>
    #include <openssl/x509.h>
    #include <openssl/x509v3.h>
    #include <openssl/x509_vfy.h>
    #include <openssl/pkcs12.h>
    #include <openssl/bio.h>
    #include <openssl/bn.h>
    #include <openssl/pem.h>

    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/Xatom.h>
    #include <X11/XKBlib.h>
    #include <X11/keysym.h>
    #include <X11/Xft/Xft.h>
    #include <fontconfig/fontconfig.h>
#endif

#endif // FIZMO_PLATFORM_INCLUDES_HPP
