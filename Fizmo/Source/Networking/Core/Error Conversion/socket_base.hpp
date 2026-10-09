#ifndef FIZMO_SOCKET_BASE_CLASS_HPP
#define FIZMO_SOCKET_BASE_CLASS_HPP

#include "addresses.hpp"
#include "socket_state.hpp"
#include <mutex>
#include "Socket Impl/native_impl.hpp"
#include "Error Conversion/native_errors.hpp"

namespace fizmo {
namespace networking {
namespace core {

class Buffer;

class SocketBase {
protected:
    SocketType m_type;
    AddressFamily m_family;
    std::unique_ptr<detail::SocketImplBase> m_impl;
    SocketState m_state;
    mutable std::mutex m_socket_mutex;

    std::vector<SocketError> m_errors;
    std::size_t m_max_errors;
    mutable std::mutex m_error_mutex;
    bool m_log_all_errors;

    unsigned int m_default_connect_timeout_ms;
    unsigned int m_default_send_timeout_ms;
    unsigned int m_default_receive_timeout_ms;

    SocketBase(SocketType type, AddressFamily family) noexcept
;

    SocketBase(
        SocketType type, AddressFamily family,
        std::unique_ptr<detail::SocketImplBase> impl,
        SocketState::State initial_state
    ) noexcept
;

    void close_internal() noexcept;

    void graceful_close_internal(unsigned int linger_ms = 2000) noexcept;

public:
    SocketBase(const SocketBase&) = delete;
    SocketBase& operator=(const SocketBase&) = delete;
    virtual ~SocketBase() = default;

    SocketBase(SocketBase&& other) noexcept
;

    SocketBase& operator=(SocketBase&& other) noexcept;

public:
    void add_error(const SocketError& error) noexcept;

    void clear_errors() noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        m_errors.clear();
    }

public:
    bool bind(const NetworkAddress& address) noexcept;

    void close() noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        close_internal();
    }

    bool shutdown(ShutdownMode mode = ShutdownMode::Both) noexcept;

    void graceful_close(unsigned int linger_ms = 2000) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        graceful_close_internal(linger_ms);
    }

public:
    int scatter_receive(detail::IOBuffer* buffers, std::size_t count) noexcept;

    int gather_send(const detail::IOBuffer* buffers, std::size_t count) noexcept;

public:
    int set_blocking(const bool blocking) noexcept;

    template<typename T>
    int set_option(const int level, const int option, const T& value) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_state.is_initialized()) return m_impl->set_option(level, option, &value, sizeof(T));
        return -1;
    }

    template<typename T>
    bool get_option(const int level, const int option, T& value) const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_impl) return false;
        int len = sizeof(T);
        return m_impl->get_option(level, option, &value, &len) == 0;
    }

    void set_connect_timeout(const unsigned int ms) noexcept;

    void set_send_timeout(const unsigned int ms) noexcept;

    void set_receive_timeout(const unsigned int ms) noexcept;

    void set_log_all_errors(const bool value) noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        m_log_all_errors = value;
    }

    void set_max_errors(const std::size_t max) noexcept;

    bool set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept;

public:
    detail::SocketImplBase* get_impl() noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        return m_impl.get();
    }

    SocketType type() const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        return m_type;
    }

    AddressFamily family() const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        return m_family;
    }

    std::vector<SocketError> errors() const noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        return m_errors;
    }

    SocketError last_error() const noexcept;

    std::size_t error_count() const noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        return m_errors.size();
    }

    bool log_all_errors() const noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        return m_log_all_errors;
    }

    SocketState::State state() const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        return m_state.state();
    }

    unsigned int connect_timeout() const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        return m_default_connect_timeout_ms;
    }

    unsigned int send_timeout() const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        return m_default_send_timeout_ms;
    }

    unsigned int receive_timeout() const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        return m_default_receive_timeout_ms;
    }

    detail::native_handle_t get_raw_socket() const noexcept;

    bool is_valid() const noexcept;

    bool is_connected() const noexcept;

    bool has_critical_errors() const noexcept;

    bool has_error_of_type(ErrorCode code) const noexcept;

public:
    NetworkAddress get_local_address() const noexcept;

    NetworkAddress get_remote_address() const noexcept;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_BASE_CLASS_HPP