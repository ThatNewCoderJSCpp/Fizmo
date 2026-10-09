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

    ~TCPSocket() override;

public:
    bool connect(const NetworkAddress& address) noexcept;

    bool listen(const int backlog = 5) noexcept;

    std::unique_ptr<TCPSocket> accept() noexcept;

    int send(const void* data, const std::size_t length) noexcept;

    int receive(void* buffer, const std::size_t length) noexcept;

    int scatter_receive(detail::IOBuffer* buffers, std::size_t count) noexcept;

    int gather_send(const detail::IOBuffer* buffers, std::size_t count) noexcept;

    int send_buffer(Buffer& buffer, const std::size_t max_length = 0) noexcept;
    int receive_to_buffer(Buffer& buffer, const std::size_t max_length = 0) noexcept;

private:
    TCPSocket(AddressFamily family, std::unique_ptr<detail::SocketImplBase> impl) noexcept : SocketBase(SocketType::TCP, family, std::move(impl), SocketState::State::Connected) {}
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TCP_SOCKET_HPP