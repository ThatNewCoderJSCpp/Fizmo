#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "raw_socket.hpp"

namespace fizmo {
namespace networking {
namespace core {

RawSocket::RawSocket(RawProtocol protocol, AddressFamily family) noexcept : SocketBase(SocketType::UDP, family), m_protocol(protocol) {
        if (m_impl) { m_impl->close(); }
        detail::native_handle_t handle = detail::sockops::create_raw_handle(family, native_protocol(protocol, family));

        if (handle == detail::kInvalidHandle) {
            m_impl.reset();
            m_state = SocketState::State::Error;
            add_error(NativeErrorConverter::get_last_error("socket(SOCK_RAW)"));
            return;
        }

        m_impl = detail::sockops::adopt(handle, SocketType::UDP, family);

        if (m_impl) {
            m_state = SocketState::State::Initialized;
        } else {
            m_state = SocketState::State::Error;
            add_error(SocketError(ErrorCode::OutOfMemory, "Could not wrap raw socket handle", 0, "RawSocket"));
        }
    }

auto RawSocket::operator=(RawSocket&& other) noexcept -> RawSocket& {
        if (this != &other) {
            SocketBase::operator=(std::move(other));
            m_protocol = other.m_protocol;
        }

        return *this;
    }

auto RawSocket::send_to(const void* data, std::size_t length, const NetworkAddress& dest) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_sendto()) return -1;
        int result = m_impl->sendto(data, length, dest);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("sendto(raw)")); }
        return result;
    }

auto RawSocket::receive_from(void* buffer, std::size_t length, NetworkAddress& source) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        int result = m_impl->recvfrom(buffer, length, source);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("recvfrom(raw)")); }
        return result;
    }

auto RawSocket::peek(void* buffer, std::size_t length, NetworkAddress& source) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        int result = detail::sockops::peek_from(m_impl.get(), buffer, length, source);
        if (result < 0) { add_error(NativeErrorConverter::get_last_error("peek(raw)")); }
        return result;
    }

auto RawSocket::set_header_included(bool enable) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;

        if (!detail::IPOptions::header_included_supported(m_family)) {
            add_error(SocketError(
                ErrorCode::OperationNotSupported,
                "Header-included mode is not available for this address family on this platform",
                0,
                "set_header_included"
            ));

            return false;
        }

        if (!detail::IPOptions::set_header_included(m_impl.get(), m_family, enable)) {
            add_error(NativeErrorConverter::get_last_error("set_header_included"));
            return false;
        }

        return true;
    }

auto RawSocket::set_ttl(int ttl) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;

        if (!detail::IPOptions::set_unicast_hops(m_impl.get(), m_family, ttl)) {
            add_error(NativeErrorConverter::get_last_error("set_ttl(raw)"));
            return false;
        }

        return true;
    }

auto RawSocket::get_ttl() const noexcept -> int {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return -1;
        return detail::IPOptions::get_unicast_hops(m_impl.get(), m_family);
    }

auto RawSocket::build_icmp_echo_request(
        std::uint16_t identifier,
        std::uint16_t sequence,
        const void* payload,
        std::size_t payload_len 
) noexcept -> std::vector<std::uint8_t> {
        std::size_t total = 8 + payload_len;
        std::vector<std::uint8_t> pkt(total, 0);
        pkt[0] = 8;
        pkt[1] = 0;
        pkt[4] = static_cast<std::uint8_t>(identifier >> 8);
        pkt[5] = static_cast<std::uint8_t>(identifier & 0xFF);
        pkt[6] = static_cast<std::uint8_t>(sequence >> 8);
        pkt[7] = static_cast<std::uint8_t>(sequence & 0xFF);
        if (payload && payload_len > 0) { std::memcpy(pkt.data() + 8, payload, payload_len); }
        std::uint32_t sum = 0;

        for (std::size_t i = 0; i < total; i += 2) {
            std::uint16_t word = static_cast<std::uint16_t>(pkt[i]) << 8;
            if (i + 1 < total) word |= pkt[i + 1];
            sum += word;
        }

        while (sum >> 16) { sum = (sum & 0xFFFF) + (sum >> 16); }
        std::uint16_t chk = static_cast<std::uint16_t>(~sum);
        pkt[2] = static_cast<std::uint8_t>(chk >> 8);
        pkt[3] = static_cast<std::uint8_t>(chk & 0xFF);
        return pkt;
    }

auto RawSocket::parse_icmp_echo_reply(
        const void* data,
        std::size_t length,
        std::uint16_t expected_id
) noexcept -> int {
        auto* bytes = static_cast<const std::uint8_t*>(data);
        if (length < 28) return -1;
        std::size_t ip_hdr_len = static_cast<std::size_t>(bytes[0] & 0x0F) * 4;
        if (length < ip_hdr_len + 8) return -1;
        const std::uint8_t* icmp = bytes + ip_hdr_len;
        if (icmp[0] != 0) return -1;
        if (icmp[1] != 0) return -1;
        std::uint16_t id = (static_cast<std::uint16_t>(icmp[4]) << 8) | icmp[5];
        if (id != expected_id) return -1;
        std::uint16_t seq = (static_cast<std::uint16_t>(icmp[6]) << 8) | icmp[7];
        return static_cast<int>(seq);
    }

auto RawSocket::native_protocol(RawProtocol proto, AddressFamily family) noexcept -> int {
        switch (proto) {
            case RawProtocol::ICMP:
                return (family == AddressFamily::IPv6) ? detail::sockops::kProtocolICMPv6 : detail::sockops::kProtocolICMP;
            case RawProtocol::ICMPv6:
                return detail::sockops::kProtocolICMPv6;
            case RawProtocol::RAW:
                return detail::sockops::kProtocolRaw;
            default:
                return detail::sockops::kProtocolICMP;
        }
    }

} // namespace core
} // namespace networking
} // namespace fizmo
