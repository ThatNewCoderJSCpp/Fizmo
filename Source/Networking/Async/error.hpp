#ifndef FIZMO_ASYNC_ERROR_HPP
#define FIZMO_ASYNC_ERROR_HPP

#include <string>
#include <system_error>

namespace fizmo {
namespace networking {
namespace async {

enum class error {
    success = 0,
    operation_aborted,
    eof,
    timed_out,
    not_connected,
    already_open,
    not_open,
    bad_descriptor,
    host_not_found,
    service_not_found,
    host_not_found_try_again,
    no_data,
    address_in_use,
    connection_refused,
    connection_reset,
    connection_aborted,
    would_block,
    in_progress,
    message_size,
    shut_down,
    invalid_argument,
    handshake_failed,
    tls_error,
    protocol_error,
    buffer_full,
    not_found,
    closed,
    limit_exceeded,
    certificate_error,
    unsupported
};

const std::error_category& async_category() noexcept;

inline std::error_code make_error_code(error e) noexcept {
    return std::error_code(static_cast<int>(e), async_category());
}

std::error_code socket_error_code(int native_code) noexcept;

std::error_code last_socket_error() noexcept;

std::error_code resolver_error_code(int gai_code) noexcept;

bool is_would_block(const std::error_code& ec) noexcept;

bool is_disconnect(const std::error_code& ec) noexcept;

} // namespace async
} // namespace networking
} // namespace fizmo

namespace std {
template <>
struct is_error_code_enum<fizmo::networking::async::error> : true_type {};
} // namespace std

#endif // FIZMO_ASYNC_ERROR_HPP
