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
    static SocketError convert(int err, const std::string& operation = "") noexcept {
        if (err == 0) { return SocketError(ErrorCode::Success, "Operation completed successfully", 0, operation); }
        ErrorCode code;
        std::string message = get_error_message(err);

        switch (err) {
            case ETIMEDOUT:
                code = ErrorCode::Timeout;
                break;
            case ENETUNREACH:
                code = ErrorCode::NetworkUnreachable;
                break;
            case ECONNREFUSED:
                code = ErrorCode::ConnectionRefused;
                break;
            case EADDRINUSE:
                code = ErrorCode::AddressInUse;
                break;
            case EADDRNOTAVAIL:
                code = ErrorCode::InvalidAddress;
                break;
            case ENOTCONN:
            case EDESTADDRREQ:
                code = ErrorCode::NotConnected;
                break;
            case ECONNRESET:
                code = ErrorCode::ConnectionReset;
                break;
            case EHOSTUNREACH:
            case EHOSTDOWN:
                code = ErrorCode::HostUnreachable;
                break;
            case EACCES:
            case EPERM:
                code = ErrorCode::AccessDenied;
                break;
            case ENOMEM:
            case ENOBUFS:
                code = ErrorCode::OutOfMemory;
                break;
            case ESHUTDOWN:
            case EPIPE:
            case EBADF:
            case ENOTSOCK:
                code = ErrorCode::SocketClosed;
                break;
            case EAGAIN:
        #if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
            case EWOULDBLOCK:
        #endif
                code = ErrorCode::WouldBlock;
                break;
            case EINPROGRESS:
            case EALREADY:
                code = ErrorCode::InProgress;
                break;
            case EINVAL:
            case EFAULT:
                code = ErrorCode::InvalidArgument;
                break;
            case EMSGSIZE:
                code = ErrorCode::MessageTooLarge;
                break;
            case EINTR:
                code = ErrorCode::OperationInterrupted;
                break;
            case EPROTONOSUPPORT:
            case EPROTOTYPE:
            case EPROTO:
                code = ErrorCode::ProtocolError;
                break;
            case ESOCKTNOSUPPORT:
                code = ErrorCode::SocketTypeNotSupported;
                break;
            case EAFNOSUPPORT:
                code = ErrorCode::AddressFamilyNotSupported;
                break;
            case EISCONN:
                code = ErrorCode::AlreadyConnected;
                break;
            case ECONNABORTED:
                code = ErrorCode::Aborted;
                break;
            case ECANCELED:
                code = ErrorCode::Aborted;
                break;
            case EOPNOTSUPP:
            case ENOPROTOOPT:
                code = ErrorCode::OperationNotSupported;
                break;
            case ENETDOWN:
                code = ErrorCode::NetworkDown;
                break;
            case EMFILE:
            case ENFILE:
                code = ErrorCode::TooManyConnections;
                break;
            case ENETRESET:
                code = ErrorCode::RemoteHostClosed;
                break;
            default:
                code = ErrorCode::UnknownError;
                break;
        }

        return SocketError(code, message, err, operation);
    }

    static SocketError get_last_error(const std::string& operation = "") noexcept { return convert(errno, operation); }

    static bool would_block() noexcept {
        return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS);
    }

    static bool is_connection_error() noexcept {
        return (errno == ECONNREFUSED || errno == ETIMEDOUT || errno == ENETUNREACH || errno == EHOSTUNREACH);
    }

    static bool is_temporary_error() noexcept {
        return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == EINTR || errno == ETIMEDOUT);
    }

    static bool is_critical_error() noexcept {
        return (errno == ECONNRESET || errno == ENETDOWN || errno == ECONNABORTED || errno == ESHUTDOWN || errno == EPIPE);
    }

    static std::string get_error_message(int error_code) noexcept {
        char buf[256];
        buf[0] = '\0';

    #if defined(__GLIBC__) && defined(_GNU_SOURCE)
        const char* msg = ::strerror_r(error_code, buf, sizeof(buf)); 
        return msg ? std::string(msg) : std::string("Unknown error");
    #else
        if (::strerror_r(error_code, buf, sizeof(buf)) != 0) { return "Unknown error"; }
        return std::string(buf);
    #endif
    }

    static SocketError convert_gai(int gai_error, const std::string& operation = "") noexcept {
        if (gai_error == 0) { return SocketError(ErrorCode::Success, "Resolution succeeded", 0, operation); }
        if (gai_error == EAI_SYSTEM) { return convert(errno, operation); }
        ErrorCode code;

        switch (gai_error) {
            case EAI_NONAME:
        #ifdef EAI_NODATA
            case EAI_NODATA:
        #endif
            case EAI_AGAIN:
            case EAI_FAIL:
                code = ErrorCode::NameResolutionFailed;
                break;
            case EAI_FAMILY:
                code = ErrorCode::AddressFamilyNotSupported;
                break;
            case EAI_SOCKTYPE:
                code = ErrorCode::SocketTypeNotSupported;
                break;
            case EAI_SERVICE:
                code = ErrorCode::ServiceUnavailable;
                break;
            case EAI_MEMORY:
                code = ErrorCode::OutOfMemory;
                break;
            case EAI_BADFLAGS:
                code = ErrorCode::InvalidArgument;
                break;
            default:
                code = ErrorCode::NameResolutionFailed;
                break;
        }

        return SocketError(code, ::gai_strerror(gai_error), gai_error, operation);
    }

    static SocketError convert_ssl_error(unsigned long ssl_error, const std::string& operation = "") noexcept {
        if (ssl_error == 0) { return SocketError(ErrorCode::Success, "SSL operation completed successfully", 0, operation); }
        return SocketError(ErrorCode::SSLError, "TLS error", static_cast<int>(ssl_error), operation);
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // OS_LINUX
#endif // FIZMO_POSIX_ERROR_CONVERTER_HPP