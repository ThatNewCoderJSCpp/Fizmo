#ifndef FIZMO_TCP_SOCKET_HPP
#define FIZMO_TCP_SOCKET_HPP

#include "socket_base.hpp"

namespace fizmo {
namespace networking {
namespace core {

class TCPSocket : public SocketBase {
public:
    explicit TCPSocket(AddressFamily family = AddressFamily::IPv4) noexcept : SocketBase(SocketType::TCP, family) {}
    TCPSocket(const TCPSocket&) = delete;
    TCPSocket& operator=(const TCPSocket&) = delete;
    TCPSocket(TCPSocket&& other) noexcept : SocketBase(std::move(other)) {}

    TCPSocket& operator=(TCPSocket&& other) noexcept {
        SocketBase::operator=(std::move(other));
        return *this;
    }

    ~TCPSocket() override {
        std::lock_guard<std::mutex> lock(m_socket_mutex);

        if (m_state.state() == SocketState::State::Connected) {
            graceful_close_internal(500); 
        } else {
            close_internal();
        }
    }

public:
    bool connect(const NetworkAddress& address) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;
        if (m_state.state() != SocketState::State::Initialized && m_state.state() != SocketState::State::Bound) return false;
        m_state = SocketState::State::Connecting;
        const bool result = m_impl->connect(address);

        if (result) {
            m_state = SocketState::State::Connected;
        } else {
            m_state = SocketState::State::Initialized;
        #ifdef OS_WINDOWS
            SocketError err = WinsockErrorConverter::get_last_error("connect");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; } 
        #endif
        }

        return result;
    }

    bool listen(const int backlog = 5) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_state.state() != SocketState::State::Bound) return false;
        const bool result = m_impl->listen(backlog);

        if (result) {
            m_state = SocketState::State::Listening;
        } else {
            m_state = SocketState::State::Error;
        #ifdef OS_WINDOWS
            SocketError err = WinsockErrorConverter::get_last_error("listen");
            add_error(err); 
        #endif
        }

        return result;
    }

    std::unique_ptr<TCPSocket> accept() noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_accept()) return nullptr;
        m_state = SocketState::State::Accepting;
        auto impl = m_impl->accept();
        m_state = SocketState::State::Listening;

        if (!impl) {
        #ifdef OS_WINDOWS
            SocketError err = WinsockErrorConverter::get_last_error("accept");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        #endif
            return nullptr;
        }
        
        AddressFamily af = impl->family();
        return std::unique_ptr<TCPSocket>(new TCPSocket(af, std::move(impl)));
    }

    int send(const void* data, const std::size_t length) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_send()) return -1;
        const int result = m_impl->send(data, length);

        if (result < 0) {
        #ifdef OS_WINDOWS
            SocketError err = WinsockErrorConverter::get_last_error("send");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        #endif
        }

        return result;
    }

    int receive(void* buffer, const std::size_t length) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_receive()) return -1;
        const int result = m_impl->receive(buffer, length);

        if (result == 0) {
            m_state = SocketState::State::Closing;
        } else if (result < 0) {
        #ifdef OS_WINDOWS
            SocketError err = WinsockErrorConverter::get_last_error("receive");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        #endif
        }

        return result;
    }

    int scatter_receive(detail::IOBuffer* buffers, std::size_t count) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_receive()) return -1;
        int result = m_impl->scatter_receive(buffers, count);

        if (result == 0) {
            m_state = SocketState::State::Closing;
        } else if (result < 0) {
        #ifdef OS_WINDOWS
            SocketError err = WinsockErrorConverter::get_last_error("scatter_receive");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        #endif
        }

        return result;
    }

    int gather_send(const detail::IOBuffer* buffers, std::size_t count) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_send()) return -1;
        int result = m_impl->gather_send(buffers, count);

        if (result < 0) {
        #ifdef OS_WINDOWS
            SocketError err = WinsockErrorConverter::get_last_error("gather_send");
            add_error(err);
            if (err.is_critical()) { m_state = SocketState::State::Error; }
        #endif
        }

        return result;
    }

    int send_buffer(Buffer& buffer, const std::size_t max_length = 0) noexcept;
    int receive_to_buffer(Buffer& buffer, const std::size_t max_length = 0) noexcept;

private:
    TCPSocket(AddressFamily family, std::unique_ptr<detail::SocketImplBase> impl) noexcept : SocketBase(SocketType::TCP, family, std::move(impl), SocketState::State::Connected) {}
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TCP_SOCKET_HPP