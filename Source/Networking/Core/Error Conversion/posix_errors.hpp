#ifndef FIZMO_POSIX_ERROR_CONVERTER_HPP
#define FIZMO_POSIX_ERROR_CONVERTER_HPP

#include "../socket_error.hpp"

#ifdef OS_LINUX

#include <cerrno>
#include <cstring>
#include <netdb.h>

namespace fizmo {
namespace networking {
namespace core {

class PosixErrorConverter {
public:
    static SocketError convert(int err, const std::string& operation = "") noexcept;

    static SocketError get_last_error(const std::string& operation = "") noexcept { return convert(errno, operation); }

    static bool would_block() noexcept {
        return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS);
    }

    static bool is_connection_error() noexcept {
        return (errno == ECONNREFUSED || errno == ETIMEDOUT || errno == ENETUNREACH || errno == EHOSTUNREACH);
    }

    static bool is_temporary_error() noexcept;

    static bool is_critical_error() noexcept;

    static std::string get_error_message(int error_code) noexcept;

    static SocketError convert_gai(int gai_error, const std::string& operation = "") noexcept;

    static SocketError convert_ssl_error(unsigned long ssl_error, const std::string& operation = "") noexcept;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // OS_LINUX
#endif // FIZMO_POSIX_ERROR_CONVERTER_HPP