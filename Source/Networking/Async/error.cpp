#include "fizmo_library.hpp"
#include "error.hpp"
#include "platform.hpp"

namespace fizmo {
namespace networking {
namespace async {

namespace {

class AsyncCategory final : public std::error_category {
public:
    const char* name() const noexcept override { return "fizmo.async"; }

    std::string message(int code) const override {
        switch (static_cast<error>(code)) {
            case error::success:                  return "Success";
            case error::operation_aborted:        return "Operation aborted";
            case error::eof:                      return "End of file";
            case error::timed_out:                return "Operation timed out";
            case error::not_connected:            return "Socket is not connected";
            case error::already_open:             return "Already open";
            case error::not_open:                 return "Not open";
            case error::bad_descriptor:           return "Bad descriptor";
            case error::host_not_found:           return "Host not found";
            case error::service_not_found:        return "Service not found";
            case error::host_not_found_try_again: return "Host not found, try again";
            case error::no_data:                  return "No data record of requested type";
            case error::address_in_use:           return "Address already in use";
            case error::connection_refused:       return "Connection refused";
            case error::connection_reset:         return "Connection reset by peer";
            case error::connection_aborted:       return "Connection aborted";
            case error::would_block:              return "Operation would block";
            case error::in_progress:              return "Operation in progress";
            case error::message_size:             return "Message too long";
            case error::shut_down:                return "Socket has been shut down";
            case error::invalid_argument:         return "Invalid argument";
            case error::handshake_failed:         return "TLS handshake failed";
            case error::tls_error:                return "TLS error";
            case error::protocol_error:           return "Protocol error";
            case error::buffer_full:              return "Buffer full";
            case error::not_found:                return "Not found";
            case error::closed:                   return "Closed";
            case error::limit_exceeded:           return "Limit exceeded";
            case error::certificate_error:        return "Certificate verification failed";
            case error::unsupported:              return "Operation not supported";
        }

        return "Unknown error";
    }

    std::error_condition default_error_condition(int code) const noexcept override {
        switch (static_cast<error>(code)) {
            case error::operation_aborted:  return std::errc::operation_canceled;
            case error::timed_out:          return std::errc::timed_out;
            case error::not_connected:      return std::errc::not_connected;
            case error::bad_descriptor:     return std::errc::bad_file_descriptor;
            case error::address_in_use:     return std::errc::address_in_use;
            case error::connection_refused: return std::errc::connection_refused;
            case error::connection_reset:   return std::errc::connection_reset;
            case error::connection_aborted: return std::errc::connection_aborted;
            case error::would_block:        return std::errc::operation_would_block;
            case error::in_progress:        return std::errc::operation_in_progress;
            case error::message_size:       return std::errc::message_size;
            case error::invalid_argument:   return std::errc::invalid_argument;
            case error::unsupported:        return std::errc::not_supported;
            default:                        return std::error_condition(code, *this);
        }
    }
};

} // namespace

const std::error_category& async_category() noexcept {
    static const AsyncCategory category;
    return category;
}

std::error_code socket_error_code(int code) noexcept {
    if (code == 0) return {};
#if defined(OS_WINDOWS)
    switch (code) {
        case WSAEWOULDBLOCK:    return error::would_block;
        case WSAEINPROGRESS:    return error::in_progress;
        case WSAECONNRESET:     return error::connection_reset;
        case WSAECONNREFUSED:   return error::connection_refused;
        case WSAECONNABORTED:   return error::connection_aborted;
        case WSAETIMEDOUT:      return error::timed_out;
        case WSAEADDRINUSE:     return error::address_in_use;
        case WSAENOTCONN:       return error::not_connected;
        case WSAESHUTDOWN:      return error::shut_down;
        case WSAEMSGSIZE:       return error::message_size;
        case WSAENOTSOCK:       return error::bad_descriptor;
        case WSAEINVAL:         return error::invalid_argument;
        case WSA_OPERATION_ABORTED:
        case WSAEINTR:          return error::operation_aborted;
        default:                return std::error_code(code, std::system_category());
    }
#else
    switch (code) {
    #if EAGAIN != EWOULDBLOCK
        case EAGAIN:
    #endif
        case EWOULDBLOCK:  return error::would_block;
        case EINPROGRESS:  return error::in_progress;
        case ECONNRESET:   return error::connection_reset;
        case EPIPE:        return error::connection_reset;
        case ECONNREFUSED: return error::connection_refused;
        case ECONNABORTED: return error::connection_aborted;
        case ETIMEDOUT:    return error::timed_out;
        case EADDRINUSE:   return error::address_in_use;
        case ENOTCONN:     return error::not_connected;
        case ESHUTDOWN:    return error::shut_down;
        case EMSGSIZE:     return error::message_size;
        case EBADF:
        case ENOTSOCK:     return error::bad_descriptor;
        case EINVAL:       return error::invalid_argument;
        case ECANCELED:    return error::operation_aborted;
        default:           return std::error_code(code, std::generic_category());
    }
#endif
}

std::error_code last_socket_error() noexcept {
#if defined(OS_WINDOWS)
    return socket_error_code(::WSAGetLastError());
#else
    return socket_error_code(errno);
#endif
}

std::error_code resolver_error_code(int gai) noexcept {
    switch (gai) {
        case 0:            return {};
        case EAI_NONAME:   return error::host_not_found;
        case EAI_AGAIN:    return error::host_not_found_try_again;
        case EAI_SERVICE:  return error::service_not_found;
    #if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
        case EAI_NODATA:   return error::no_data;
    #endif
        case EAI_FAMILY:   return error::unsupported;
    #if defined(EAI_SYSTEM)
        case EAI_SYSTEM:   return last_socket_error();
    #endif
        default:           return error::host_not_found;
    }
}

bool is_would_block(const std::error_code& ec) noexcept {
    return ec == error::would_block || ec == error::in_progress;
}

bool is_disconnect(const std::error_code& ec) noexcept {
    return ec == error::eof || ec == error::connection_reset || ec == error::connection_aborted || ec == error::not_connected || ec == error::shut_down;
}

} // namespace async
} // namespace networking
} // namespace fizmo
