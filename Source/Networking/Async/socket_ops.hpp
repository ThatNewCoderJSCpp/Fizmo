#ifndef FIZMO_ASYNC_SOCKET_OPS_HPP
#define FIZMO_ASYNC_SOCKET_OPS_HPP

#include "platform.hpp"
#include "error.hpp"

#include <cstddef>
#include <string>
#include <system_error>

namespace fizmo {
namespace networking {
namespace async {

class Endpoint;

enum class ShutdownType {
    Receive,
    Send,
    Both
};

enum class WaitType {
    Read,
    Write
};

namespace detail {

void ensure_socket_runtime() noexcept;

unsigned interface_index(const std::string& name) noexcept;

native_socket_t open_socket(int family, int type, int protocol, std::error_code& ec) noexcept;

void close_socket(native_socket_t s) noexcept;

bool set_nonblocking(native_socket_t s, bool enable, std::error_code& ec) noexcept;

bool set_option(native_socket_t s, int level, int name, const void* value, std::size_t len, std::error_code& ec) noexcept;

bool get_option(native_socket_t s, int level, int name, void* value, std::size_t& len, std::error_code& ec) noexcept;

bool set_int_option(native_socket_t s, int level, int name, int value, std::error_code& ec) noexcept;

int get_int_option(native_socket_t s, int level, int name, std::error_code& ec) noexcept;

std::error_code pending_error(native_socket_t s) noexcept;

bool bind(native_socket_t s, const Endpoint& ep, std::error_code& ec) noexcept;

bool listen(native_socket_t s, int backlog, std::error_code& ec) noexcept;

bool start_connect(native_socket_t s, const Endpoint& ep, std::error_code& ec) noexcept;

native_socket_t accept(native_socket_t s, Endpoint* peer, std::error_code& ec) noexcept;

long recv(native_socket_t s, void* data, std::size_t len, int flags, std::error_code& ec) noexcept;

long send(native_socket_t s, const void* data, std::size_t len, int flags, std::error_code& ec) noexcept;

long recv_from(native_socket_t s, void* data, std::size_t len, int flags, Endpoint& from, std::error_code& ec) noexcept;

long send_to(native_socket_t s, const void* data, std::size_t len, int flags, const Endpoint& to, std::error_code& ec) noexcept;

bool shutdown(native_socket_t s, ShutdownType how, std::error_code& ec) noexcept;

bool local_endpoint(native_socket_t s, Endpoint& out, std::error_code& ec) noexcept;

bool remote_endpoint(native_socket_t s, Endpoint& out, std::error_code& ec) noexcept;

std::size_t available(native_socket_t s, std::error_code& ec) noexcept;

int wait(native_socket_t s, WaitType type, int timeout_ms, std::error_code& ec) noexcept;

void disable_udp_connreset(native_socket_t s) noexcept;

} // namespace detail
} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_SOCKET_OPS_HPP
