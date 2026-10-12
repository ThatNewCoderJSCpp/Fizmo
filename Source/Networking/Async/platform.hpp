#ifndef FIZMO_ASYNC_PLATFORM_HPP
#define FIZMO_ASYNC_PLATFORM_HPP

#include "../../Basic/fizmo_defines.hpp"

#if defined(OS_WINDOWS)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <errno.h>
#endif

#include <cstdint>

namespace fizmo {
namespace networking {
namespace async {

#if defined(OS_WINDOWS)
using native_socket_t = SOCKET;
using socklen_type    = int;
constexpr native_socket_t invalid_socket = INVALID_SOCKET;
#else
using native_socket_t = int;
using socklen_type    = socklen_t;
constexpr native_socket_t invalid_socket = -1;
#endif

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_PLATFORM_HPP
