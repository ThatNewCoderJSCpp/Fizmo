#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "tcp_socket.hpp"

namespace fizmo {
namespace networking {
namespace core {

TCPSocket::~TCPSocket() {
        std::lock_guard<std::mutex> lock(m_socket_mutex);

        if (m_state.state() == SocketState::State::Connected) {
            graceful_close_internal(500);
        } else {
            close_internal();
        }
    }

auto TCPSocket::connect(const NetworkAddress& address) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;
        if (m_state.state() != SocketState::State::Initialized && m_state.state() != SocketState::State::Bound) return false;
        m_state = SocketState::State::Connecting;
        const bool result = m_impl->connect(address);

        if (result) {
            m_state = SocketState::State::Connected;
        } else {
            m_state = SocketState::State::Initialized;
            SocketError err = NativeErrorConverter::get_last_error("connect");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        }

        return result;
    }

auto TCPSocket::listen(const int backlog) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_state.state() != SocketState::State::Bound) return false;
        const bool result = m_impl->listen(backlog);

        if (result) {
            m_state = SocketState::State::Listening;
        } else {
            m_state = SocketState::State::Error;
            add_error(NativeErrorConverter::get_last_error("listen"));
        }

        return result;
    }

auto TCPSocket::accept() noexcept -> std::unique_ptr<TCPSocket> {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_accept()) return nullptr;
        m_state = SocketState::State::Accepting;
        auto impl = m_impl->accept();
        m_state = SocketState::State::Listening;

        if (!impl) {
            SocketError err = NativeErrorConverter::get_last_error("accept");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
            return nullptr;
        }

        AddressFamily af = impl->family();
        return std::unique_ptr<TCPSocket>(new TCPSocket(af, std::move(impl)));
    }

auto TCPSocket::send(const void* data, const std::size_t length) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_send()) return -1;
        const int result = m_impl->send(data, length);

        if (result < 0) {
            SocketError err = NativeErrorConverter::get_last_error("send");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        }

        return result;
    }

auto TCPSocket::receive(void* buffer, const std::size_t length) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_receive()) return -1;
        const int result = m_impl->receive(buffer, length);

        if (result == 0) {
            m_state = SocketState::State::Closing;
        } else if (result < 0) {
            SocketError err = NativeErrorConverter::get_last_error("receive");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        }

        return result;
    }

auto TCPSocket::scatter_receive(detail::IOBuffer* buffers, std::size_t count) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_receive()) return -1;
        int result = m_impl->scatter_receive(buffers, count);

        if (result == 0) {
            m_state = SocketState::State::Closing;
        } else if (result < 0) {
            SocketError err = NativeErrorConverter::get_last_error("scatter_receive");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        }

        return result;
    }

auto TCPSocket::gather_send(const detail::IOBuffer* buffers, std::size_t count) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_send()) return -1;
        int result = m_impl->gather_send(buffers, count);

        if (result < 0) {
            SocketError err = NativeErrorConverter::get_last_error("gather_send");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        }

        return result;
    }

} // namespace core
} // namespace networking
} // namespace fizmo
