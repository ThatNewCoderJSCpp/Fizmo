#ifndef FIZMO_SOCKET_BASE_CLASS_HPP
#define FIZMO_SOCKET_BASE_CLASS_HPP

#include "addresses.hpp"
#include "socket_state.hpp"
#include <mutex>

#ifdef OS_WINDOWS
#include "Socket Impl/winsock_impl.hpp"
#include "Error Conversion/winsock_errors.hpp"
#endif

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
        : m_type(type), m_family(family),
          m_state(SocketState::State::Uninitialized),
          m_max_errors(std::numeric_limits<std::size_t>::max() / 2),
          m_log_all_errors(true),
          m_default_connect_timeout_ms(30000),
          m_default_send_timeout_ms(30000),
          m_default_receive_timeout_ms(30000)
    {
    #ifdef OS_WINDOWS
        m_impl = std::make_unique<detail::WinsockImpl>(type, family);
    #endif

        if (m_impl) {
            m_state = SocketState::State::Initialized;
            m_impl->set_connect_timeout(m_default_connect_timeout_ms);
            m_impl->set_send_timeout(m_default_send_timeout_ms);
            m_impl->set_receive_timeout(m_default_receive_timeout_ms);
        } else {
            m_state = SocketState::State::Error;
        }
    }

    SocketBase(
        SocketType type, AddressFamily family,
        std::unique_ptr<detail::SocketImplBase> impl,
        SocketState::State initial_state
    ) noexcept
        : m_type(type), m_family(family), m_impl(std::move(impl)),
          m_state(initial_state),
          m_max_errors(std::numeric_limits<std::size_t>::max() / 2),
          m_log_all_errors(true),
          m_default_connect_timeout_ms(30000),
          m_default_send_timeout_ms(30000),
          m_default_receive_timeout_ms(30000) {}

    void close_internal() noexcept {
        if (m_state.is_initialized() && m_state.state() != SocketState::State::Closed) {
            m_impl->close();
            m_state = SocketState::State::Closed;
        }
    }

    void graceful_close_internal(unsigned int linger_ms = 2000) noexcept {
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return;
        m_state = SocketState::State::Closing;  
        m_impl->shutdown(ShutdownMode::Write);

        if (m_type == SocketType::TCP && m_impl->is_connected()) {
            char drain_buf[1024];
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(linger_ms);

            while (std::chrono::steady_clock::now() < deadline) {
                int n = m_impl->receive(drain_buf, sizeof(drain_buf));
                if (n <= 0) break;  
            }
        }

        m_impl->close();
        m_state = SocketState::State::Closed;
    }

public:
    SocketBase(const SocketBase&) = delete;
    SocketBase& operator=(const SocketBase&) = delete;
    virtual ~SocketBase() = default;

    SocketBase(SocketBase&& other) noexcept
        : m_type(SocketType::TCP), m_family(AddressFamily::IPv4),
          m_state(SocketState::State::Uninitialized),
          m_max_errors(0), m_log_all_errors(true),
          m_default_connect_timeout_ms(0),
          m_default_send_timeout_ms(0),
          m_default_receive_timeout_ms(0)
    {
        std::unique_lock<std::mutex> lock_socket(other.m_socket_mutex, std::defer_lock);
        std::unique_lock<std::mutex> lock_error(other.m_error_mutex, std::defer_lock);
        std::lock(lock_socket, lock_error);

        m_type   = other.m_type;
        m_family = other.m_family;
        m_impl   = std::move(other.m_impl);
        m_state  = other.m_state;
        m_errors = std::move(other.m_errors);
        m_max_errors     = other.m_max_errors;
        m_log_all_errors = other.m_log_all_errors;
        m_default_connect_timeout_ms = other.m_default_connect_timeout_ms;
        m_default_send_timeout_ms    = other.m_default_send_timeout_ms;
        m_default_receive_timeout_ms = other.m_default_receive_timeout_ms;

        other.m_state = SocketState::State::Closed;
    }

    SocketBase& operator=(SocketBase&& other) noexcept {
        if (this != &other) {
            std::unique_lock<std::mutex> lk1(m_socket_mutex, std::defer_lock);
            std::unique_lock<std::mutex> lk2(other.m_socket_mutex, std::defer_lock);
            std::unique_lock<std::mutex> lk3(m_error_mutex, std::defer_lock);
            std::unique_lock<std::mutex> lk4(other.m_error_mutex, std::defer_lock);
            std::lock(lk1, lk2, lk3, lk4);

            close_internal();

            m_type   = other.m_type;
            m_family = other.m_family;
            m_impl   = std::move(other.m_impl);
            m_state  = other.m_state;
            m_errors = std::move(other.m_errors);
            m_max_errors     = other.m_max_errors;
            m_log_all_errors = other.m_log_all_errors;
            m_default_connect_timeout_ms = other.m_default_connect_timeout_ms;
            m_default_send_timeout_ms    = other.m_default_send_timeout_ms;
            m_default_receive_timeout_ms = other.m_default_receive_timeout_ms;

            other.m_state = SocketState::State::Uninitialized;
        }
        return *this;
    }

public:
    void add_error(const SocketError& error) noexcept {
        if (!m_log_all_errors && !error.is_critical() && error.is_temporary()) return;
        std::lock_guard<std::mutex> lock(m_error_mutex);
        if (m_errors.size() >= m_max_errors) { m_errors.erase(m_errors.begin()); }
        m_errors.push_back(error);
    }

    void clear_errors() noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        m_errors.clear();
    }

public:
    bool bind(const NetworkAddress& address) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;
        if (m_state.state() != SocketState::State::Initialized) return false;
        m_state = SocketState::State::Binding;
        const bool result = m_impl->bind(address);

        if (result) {
            m_state = SocketState::State::Bound;
        } else {
            m_state = SocketState::State::Initialized;

        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("bind"));
        #endif
        }

        return result;
    }

    void close() noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        close_internal();
    }

    bool shutdown(ShutdownMode mode = ShutdownMode::Both) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;
        bool result = m_impl->shutdown(mode);

        if (!result) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("shutdown"));
        #endif
        }

        if (mode == ShutdownMode::Both) { m_state = SocketState::State::Closing; }
        return result;
    }

    void graceful_close(unsigned int linger_ms = 2000) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        graceful_close_internal(linger_ms);
    }

public:
    int scatter_receive(detail::IOBuffer* buffers, std::size_t count) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_receive()) return -1;
        int result = m_impl->scatter_receive(buffers, count);

        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("scatter_receive"));
        #endif
        } else if (result == 0) {
            m_state = SocketState::State::Closing;
        }
        return result;
    }

    int gather_send(const detail::IOBuffer* buffers, std::size_t count) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_send()) return -1;
        int result = m_impl->gather_send(buffers, count);
        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("gather_send"));
        #endif
        }
        return result;
    }

public:
    int set_blocking(const bool blocking) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_state.is_initialized()) return m_impl->set_blocking(blocking);
        return -1;
    }

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

    void set_connect_timeout(const unsigned int ms) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        m_default_connect_timeout_ms = ms;
        if (m_impl) m_impl->set_connect_timeout(ms);
    }

    void set_send_timeout(const unsigned int ms) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        m_default_send_timeout_ms = ms;
        if (m_impl) m_impl->set_send_timeout(ms);
    }

    void set_receive_timeout(const unsigned int ms) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        m_default_receive_timeout_ms = ms;
        if (m_impl) m_impl->set_receive_timeout(ms);
    }

    void set_log_all_errors(const bool value) noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        m_log_all_errors = value;
    }

    void set_max_errors(const std::size_t max) noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        m_max_errors = max;

        if (m_errors.size() > m_max_errors) {
            m_errors.erase(m_errors.begin(), m_errors.begin() + (m_errors.size() - m_max_errors));
        }
    }

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

    SocketError last_error() const noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        return m_errors.empty() ? SocketError() : m_errors.back();
    }

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

#ifdef OS_WINDOWS
    SOCKET get_raw_socket() const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return INVALID_SOCKET;
        auto* w = dynamic_cast<detail::WinsockImpl*>(m_impl.get());
        return w ? w->get_raw_socket() : INVALID_SOCKET;
    }
#endif

    bool is_valid() const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        return m_state.is_initialized() && m_state.state() != SocketState::State::Closed && m_state.state() != SocketState::State::Error;
    }

    bool is_connected() const noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        return m_state.state() == SocketState::State::Connected && m_impl && m_impl->is_connected();
    }

    bool has_critical_errors() const noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        for (const auto& e : m_errors) { if (e.is_critical()) return true; }
        return false;
    }

    bool has_error_of_type(ErrorCode code) const noexcept {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        for (const auto& e : m_errors) { if (e.code() == code) return true; }
        return false;
    }

public:
    NetworkAddress get_local_address() const noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return {};
        auto* w = dynamic_cast<detail::WinsockImpl*>(m_impl.get());
        if (!w) return {};
        SOCKET raw = w->get_raw_socket();
        if (raw == INVALID_SOCKET) return {};
        struct sockaddr_storage storage;
        int addr_len = sizeof(storage);
        std::memset(&storage, 0, sizeof(storage));

        if (::getsockname(raw, reinterpret_cast<struct sockaddr*>(&storage), &addr_len) != 0) {
            return {};
        }

        return detail::WinsockImpl::parse_sockaddr(storage);
    #else
        return {};
    #endif
    }

    NetworkAddress get_remote_address() const noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_state.state() != SocketState::State::Connected) return {};
        auto* w = dynamic_cast<detail::WinsockImpl*>(m_impl.get());
        if (!w) return {};
        SOCKET raw = w->get_raw_socket();
        if (raw == INVALID_SOCKET) return {};
        struct sockaddr_storage storage;
        int addr_len = sizeof(storage);
        std::memset(&storage, 0, sizeof(storage));

        if (::getpeername(raw, reinterpret_cast<struct sockaddr*>(&storage), &addr_len) != 0) {
            return {};
        }

        return detail::WinsockImpl::parse_sockaddr(storage);
    #else
        return {};
    #endif
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_BASE_CLASS_HPP