#ifndef FIZMO_WINSOCK_ERROR_CONVERTER_HPP
#define FIZMO_WINSOCK_ERROR_CONVERTER_HPP

#include "../socket_error.hpp"

#ifdef OS_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <schannel.h>
#include <security.h>

namespace fizmo {
namespace networking {
namespace core {

class WinsockErrorConverter {
public:
    static SocketError convert(int win_error, const std::string& operation = "") noexcept;
    
    static SocketError get_last_error(const std::string& operation = "") noexcept;
    
    static bool would_block() noexcept;
    
    static bool is_connection_error() noexcept;
    
    static bool is_temporary_error() noexcept;
    
    static bool is_critical_error() noexcept;
    
    static std::string get_error_message(int error_code) noexcept;

    static SocketError convert_ssl_error(SECURITY_STATUS sec_status, const std::string& operation = "") noexcept;
    
    static bool is_certificate_error(SECURITY_STATUS status) noexcept;
    
    static bool is_network_unreachable() noexcept;
    static bool is_host_unreachable() noexcept;
    static bool is_connection_refused() noexcept;
    static bool is_address_in_use() noexcept;

    static bool ssl_needs_more_data() noexcept;

    static bool ssl_needs_renegotiation() noexcept;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // OS_WINDOWS
#endif // FIZMO_WINSOCK_ERROR_CONVERTER_HPP