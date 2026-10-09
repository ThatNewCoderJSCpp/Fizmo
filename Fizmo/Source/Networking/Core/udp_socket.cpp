#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "udp_socket.hpp"

namespace fizmo {
namespace networking {
namespace core {

auto UDPSocket::connect(const NetworkAddress& address) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_sendto()) return false;
        SocketState::State prev = m_state.state();
        m_state = SocketState::State::Connecting;
        const bool result = m_impl->connect(address);

        if (result) {
            m_state = SocketState::State::Connected;
        } else {
            m_state = prev;
            add_error(NativeErrorConverter::get_last_error("connect"));
        }

        return result;
    }

auto UDPSocket::disconnect() noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_state.state() != SocketState::State::Connected) return false;

        if (detail::sockops::dissolve_association(m_impl.get())) {
            m_state = SocketState::State::Bound;
            return true;
        }

        add_error(NativeErrorConverter::get_last_error("disconnect"));
        return false;
    }

auto UDPSocket::send_connected(const void* data, const std::size_t length) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_send()) return -1;
        const int result = m_impl->send(data, length);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("send")); }
        return result;
    }

auto UDPSocket::receive_connected(void* buffer, const std::size_t length) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_receive()) return -1;
        const int result = m_impl->receive(buffer, length);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("receive")); }
        return result;
    }

auto UDPSocket::send(const void* data, const std::size_t length, const NetworkAddress& dest) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_sendto()) return -1;
        const int result = m_impl->sendto(data, length, dest);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("sendto")); }
        return result;
    }

auto UDPSocket::receive(void* buffer, const std::size_t length, NetworkAddress& source) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        const int result = m_impl->recvfrom(buffer, length, source);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("recvfrom")); }
        return result;
    }

auto UDPSocket::receive_string(std::size_t max_length) noexcept -> std::pair<std::string, NetworkAddress> {
        std::string buf(max_length, '\0');
        NetworkAddress source;
        int n = receive(&buf[0], max_length, source);
        if (n <= 0) return {{}, {}};
        buf.resize(static_cast<std::size_t>(n));
        return {std::move(buf), source};
    }

auto UDPSocket::receive_string_connected(std::size_t max_length) noexcept -> std::string {
        std::string buf(max_length, '\0');
        int n = receive_connected(&buf[0], max_length);
        if (n <= 0) return {};
        buf.resize(static_cast<std::size_t>(n));
        return buf;
    }

auto UDPSocket::broadcast(const void* data, std::size_t length, std::uint16_t port) noexcept -> int {
        options(*this).set(BooleanOption::Broadcast, true);
        NetworkAddress dest("255.255.255.255", port);
        return send(data, length, dest);
    }

auto UDPSocket::peek(void* buffer, std::size_t length, NetworkAddress& source) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        int result = detail::sockops::peek_from(m_impl.get(), buffer, length, source);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("peek")); }
        return result;
    }

auto UDPSocket::peek_connected(void* buffer, std::size_t length) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_receive()) return -1;
        int result = detail::sockops::peek(m_impl.get(), buffer, length);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("peek_connected")); }
        return result;
    }

auto UDPSocket::set_max_hops(int hops) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;

        if (!detail::IPOptions::set_unicast_hops(m_impl.get(), m_family, hops)) {
            add_error(NativeErrorConverter::get_last_error("set_max_hops"));
            return false;
        }

        return true;
    }

auto UDPSocket::get_max_hops() const noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return -1;
        return detail::IPOptions::get_unicast_hops(m_impl.get(), m_family);
    }

auto UDPSocket::scatter_receive(detail::IOBuffer* buffers, std::size_t count, NetworkAddress& source) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        int result = m_impl->scatter_recvfrom(buffers, count, source);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("scatter_recvfrom")); }
        return result;
    }

auto UDPSocket::gather_send(const detail::IOBuffer* buffers, std::size_t count, const NetworkAddress& dest) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_sendto()) return -1;
        int result = m_impl->gather_sendto(buffers, count, dest);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("gather_sendto")); }
        return result;
    }

auto UDPSocket::join_multicast_group(const std::string& group_address, const std::string& local_interface) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;

        if (!detail::IPOptions::join_group(m_impl.get(), m_family, group_address, local_interface)) {
            add_error(NativeErrorConverter::get_last_error("join_multicast_group"));
            return false;
        }

        return true;
    }

auto UDPSocket::leave_multicast_group(const std::string& group_address, const std::string& local_interface) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;

        if (!detail::IPOptions::leave_group(m_impl.get(), m_family, group_address, local_interface)) {
            add_error(NativeErrorConverter::get_last_error("leave_multicast_group"));
            return false;
        }

        return true;
    }

auto UDPSocket::join_source_group(
        const std::string& group_address,
        const std::string& source_address,
        const std::string& local_interface 
) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;

        if (!detail::IPOptions::join_source_group(m_impl.get(), m_family, group_address, source_address, local_interface)) {
            add_error(NativeErrorConverter::get_last_error("join_source_group"));
            return false;
        }

        return true;
    }

auto UDPSocket::leave_source_group(
        const std::string& group_address,
        const std::string& source_address,
        const std::string& local_interface 
) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;

        if (!detail::IPOptions::leave_source_group(m_impl.get(), m_family, group_address, source_address, local_interface)) {
            add_error(NativeErrorConverter::get_last_error("leave_source_group"));
            return false;
        }

        return true;
    }

auto UDPSocket::set_multicast_ttl(int ttl) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;

        if (!detail::IPOptions::set_multicast_ttl(m_impl.get(), m_family, ttl)) {
            add_error(NativeErrorConverter::get_last_error("set_multicast_ttl"));
            return false;
        }

        return true;
    }

auto UDPSocket::set_multicast_loopback(bool enable) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;

        if (!detail::IPOptions::set_multicast_loopback(m_impl.get(), m_family, enable)) {
            add_error(NativeErrorConverter::get_last_error("set_multicast_loopback"));
            return false;
        }

        return true;
    }

auto UDPSocket::set_multicast_interface(const std::string& local_interface) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;

        if (!detail::IPOptions::set_multicast_interface(m_impl.get(), m_family, local_interface)) {
            add_error(SocketError(
                ErrorCode::InvalidArgument,
                "Could not apply multicast interface \"" + local_interface + "\"",
                0,
                "set_multicast_interface"
            ));
            return false;
        }

        return true;
    }

} // namespace core
} // namespace networking
} // namespace fizmo
