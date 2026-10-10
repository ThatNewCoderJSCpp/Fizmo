#include "fizmo_library.hpp"
#include "socket_base.hpp"

namespace fizmo {
namespace networking {
namespace core {

SocketBase::SocketBase(SocketType type, AddressFamily family) noexcept : m_type(type), m_family(family),
      m_state(SocketState::State::Uninitialized),
      m_max_errors(std::numeric_limits<std::size_t>::max() / 2),
      m_log_all_errors(true),
      m_default_connect_timeout_ms(30000),
      m_default_send_timeout_ms(30000),
      m_default_receive_timeout_ms(30000)
{
    m_impl = std::make_unique<detail::NativeSocketImpl>(type, family);

    if (m_impl) {
        m_state = SocketState::State::Initialized;
        m_impl->set_connect_timeout(m_default_connect_timeout_ms);
        m_impl->set_send_timeout(m_default_send_timeout_ms);
        m_impl->set_receive_timeout(m_default_receive_timeout_ms);
    } else {
        m_state = SocketState::State::Error;
    }
}

SocketBase::SocketBase(
        SocketType type, AddressFamily family,
        std::unique_ptr<detail::SocketImplBase> impl,
        SocketState::State initial_state
) noexcept : m_type(type), m_family(family), m_impl(std::move(impl)),
          m_state(initial_state),
          m_max_errors(std::numeric_limits<std::size_t>::max() / 2),
          m_log_all_errors(true),
          m_default_connect_timeout_ms(30000),
          m_default_send_timeout_ms(30000),
          m_default_receive_timeout_ms(30000) {}

void SocketBase::close_internal() noexcept {
    if (m_state.is_initialized() && m_state.state() != SocketState::State::Closed) {
        m_impl->close();
        m_state = SocketState::State::Closed;
    }
}

void SocketBase::graceful_close_internal(unsigned int linger_ms) noexcept {
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

SocketBase::SocketBase(SocketBase&& other) noexcept : m_type(SocketType::TCP), m_family(AddressFamily::IPv4),
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

auto SocketBase::operator=(SocketBase&& other) noexcept -> SocketBase& {
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

void SocketBase::add_error(const SocketError& error) noexcept {
    if (!m_log_all_errors && !error.is_critical() && error.is_temporary()) return;
    std::lock_guard<std::mutex> lock(m_error_mutex);
    if (m_errors.size() >= m_max_errors) { m_errors.erase(m_errors.begin()); }
    m_errors.push_back(error);
}

bool SocketBase::bind(const NetworkAddress& address) noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if (!m_state.is_initialized()) return false;
    if (m_state.state() != SocketState::State::Initialized) return false;
    m_state = SocketState::State::Binding;
    const bool result = m_impl->bind(address);

    if (result) {
        m_state = SocketState::State::Bound;
    } else {
        m_state = SocketState::State::Initialized;
        add_error(NativeErrorConverter::get_last_error("bind"));
    }

    return result;
}

bool SocketBase::shutdown(ShutdownMode mode) noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;
    bool result = m_impl->shutdown(mode);
    if (!result) { add_error(NativeErrorConverter::get_last_error("shutdown")); }
    if (mode == ShutdownMode::Both) { m_state = SocketState::State::Closing; }
    return result;
}

int SocketBase::scatter_receive(detail::IOBuffer* buffers, std::size_t count) noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if (!m_state.can_receive()) return -1;
    int result = m_impl->scatter_receive(buffers, count);

    if (result < 0) {
        add_error(NativeErrorConverter::get_last_error("scatter_receive"));
    } else if (result == 0) {
        m_state = SocketState::State::Closing;
    }

    return result;
}

int SocketBase::gather_send(const detail::IOBuffer* buffers, std::size_t count) noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if (!m_state.can_send()) return -1;
    int result = m_impl->gather_send(buffers, count);
    if (result < 0) { add_error(NativeErrorConverter::get_last_error("gather_send")); }
    return result;
}

int SocketBase::set_blocking(const bool blocking) noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if (m_state.is_initialized()) return m_impl->set_blocking(blocking);
    return -1;
}

void SocketBase::set_connect_timeout(const unsigned int ms) noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    m_default_connect_timeout_ms = ms;
    if (m_impl) m_impl->set_connect_timeout(ms);
}

void SocketBase::set_send_timeout(const unsigned int ms) noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    m_default_send_timeout_ms = ms;
    if (m_impl) m_impl->set_send_timeout(ms);
}

void SocketBase::set_receive_timeout(const unsigned int ms) noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    m_default_receive_timeout_ms = ms;
    if (m_impl) m_impl->set_receive_timeout(ms);
}

void SocketBase::set_max_errors(const std::size_t max) noexcept {
    std::lock_guard<std::mutex> lock(m_error_mutex);
    m_max_errors = max;

    if (m_errors.size() > m_max_errors) {
        m_errors.erase(m_errors.begin(), m_errors.begin() + (m_errors.size() - m_max_errors));
    }
}

bool SocketBase::set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if (!m_impl) return false;
    return m_impl->set_keepalive_params(idle_ms, interval_ms) == 0;
}

auto SocketBase::last_error() const noexcept -> SocketError {
    std::lock_guard<std::mutex> lock(m_error_mutex);
    return m_errors.empty() ? SocketError() : m_errors.back();
}

auto SocketBase::get_raw_socket() const noexcept -> detail::native_handle_t {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if (!m_state.is_initialized() || !m_impl) return detail::kInvalidHandle;
    return m_impl->native_handle();
}

bool SocketBase::is_valid() const noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    return m_state.is_initialized() && m_state.state() != SocketState::State::Closed && m_state.state() != SocketState::State::Error;
}

bool SocketBase::is_connected() const noexcept {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    return m_state.state() == SocketState::State::Connected && m_impl && m_impl->is_connected();
}

bool SocketBase::has_critical_errors() const noexcept {
    std::lock_guard<std::mutex> lock(m_error_mutex);
    for (const auto& e : m_errors) { if (e.is_critical()) return true; }
    return false;
}

bool SocketBase::has_error_of_type(ErrorCode code) const noexcept {
    std::lock_guard<std::mutex> lock(m_error_mutex);
    for (const auto& e : m_errors) { if (e.code() == code) return true; }
    return false;
}

auto SocketBase::get_local_address() const noexcept -> NetworkAddress {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if (!m_state.is_initialized() || !m_impl) return {};
    detail::native_handle_t raw = m_impl->native_handle();
    if (raw == detail::kInvalidHandle) return {};
    struct sockaddr_storage storage;
    std::memset(&storage, 0, sizeof(storage));
    detail::socklen_type addr_len = sizeof(storage);

    if (::getsockname(raw, reinterpret_cast<struct sockaddr*>(&storage), &addr_len) != 0) {
        return {};
    }

    return detail::NativeSocketImpl::parse_sockaddr(storage);
}

auto SocketBase::get_remote_address() const noexcept -> NetworkAddress {
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if (m_state.state() != SocketState::State::Connected) return {};
    auto* impl = dynamic_cast<detail::NativeSocketImpl*>(m_impl.get());
    if (!impl) return {};
    detail::native_handle_t raw = impl->get_raw_socket();
    if (raw == detail::kInvalidHandle) return {};
    struct sockaddr_storage storage;
    std::memset(&storage, 0, sizeof(storage));
    detail::socklen_type addr_len = sizeof(storage);

    if (::getpeername(raw, reinterpret_cast<struct sockaddr*>(&storage), &addr_len) != 0) {
        return {};
    }

    return detail::NativeSocketImpl::parse_sockaddr(storage);
}

} // namespace core
} // namespace networking
} // namespace fizmo
