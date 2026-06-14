#ifndef FIZMO_RAW_SOCKET_HPP
#define FIZMO_RAW_SOCKET_HPP

#include "socket_base.hpp"
#include "socket_options.hpp"

namespace fizmo {
namespace networking {
namespace core {

enum class RawProtocol {
    ICMP = 0,
    ICMPv6,
    RAW        
};

class RawSocket : public SocketBase {
private:
    RawProtocol m_protocol;

public:
    explicit RawSocket(RawProtocol protocol = RawProtocol::ICMP, AddressFamily family = AddressFamily::IPv4) noexcept : SocketBase(SocketType::UDP, family), m_protocol(protocol) {                                           
    #ifdef OS_WINDOWS
        if (m_impl) { m_impl->close(); }
        int af    = (family == AddressFamily::IPv6) ? AF_INET6 : AF_INET;
        int proto = native_protocol(protocol, family);
        SOCKET s  = ::socket(af, SOCK_RAW, proto);

        if (s == INVALID_SOCKET) {
            m_state = SocketState::State::Error;
            add_error(WinsockErrorConverter::get_last_error("socket(SOCK_RAW)"));
        } else {
            m_impl = std::make_unique<detail::WinsockImpl>(s, SocketType::UDP, family);
            m_state = SocketState::State::Initialized;
        }
    #endif
    }

    RawSocket(const RawSocket&) = delete;
    RawSocket& operator=(const RawSocket&) = delete;

    RawSocket(RawSocket&& other) noexcept : SocketBase(std::move(other)), m_protocol(other.m_protocol) {}

    RawSocket& operator=(RawSocket&& other) noexcept {
        if (this != &other) {
            SocketBase::operator=(std::move(other));
            m_protocol = other.m_protocol;
        }

        return *this;
    }

    ~RawSocket() override = default;

public:
    RawProtocol protocol() const noexcept { return m_protocol; }

public:
    int send_to(const void* data, std::size_t length, const NetworkAddress& dest) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_sendto()) return -1;
        int result = m_impl->sendto(data, length, dest);

        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("sendto(raw)"));
        #endif
        }

        return result;
    }

    int receive_from(void* buffer, std::size_t length, NetworkAddress& source) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        int result = m_impl->recvfrom(buffer, length, source);

        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("recvfrom(raw)"));
        #endif
        }

        return result;
    }

    int peek(void* buffer, std::size_t length, NetworkAddress& source) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        struct sockaddr_storage storage;
        int addr_len = sizeof(storage);
        std::memset(&storage, 0, sizeof(storage));

        int result = ::recvfrom(
            static_cast<detail::WinsockImpl*>(m_impl.get())->get_raw_socket(),
            static_cast<char*>(buffer),
            static_cast<int>(length),
            MSG_PEEK,
            reinterpret_cast<struct sockaddr*>(&storage),
            &addr_len
        );

        if (result >= 0) {
            source = detail::WinsockImpl::parse_sockaddr(storage);
        } else {
            add_error(WinsockErrorConverter::get_last_error("peek(raw)"));
        }

        return result;
    #else
        return -1;
    #endif
    }

public:
    bool set_header_included(bool enable) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;
        int val = enable ? 1 : 0;
        int result;

        if (m_family == AddressFamily::IPv6) {
            result = m_impl->set_option(IPPROTO_IPV6, IPV6_HDRINCL, &val, sizeof(val));
        } else {
            result = m_impl->set_option(IPPROTO_IP, IP_HDRINCL, &val, sizeof(val));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("set_header_included"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

public:
    bool set_ttl(int ttl) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;
        int result;

        if (m_family == AddressFamily::IPv6) {
            result = m_impl->set_option(IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl));
        } else {
            result = m_impl->set_option(IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("set_ttl(raw)"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

    int get_ttl() const noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return -1;
        int ttl = -1;
        int len = sizeof(ttl);

        if (m_family == AddressFamily::IPv6) {
            m_impl->get_option(IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl, &len);
        } else {
            m_impl->get_option(IPPROTO_IP, IP_TTL, &ttl, &len);
        }

        return ttl;
    #else
        return -1;
    #endif
    }

public:
    static std::vector<std::uint8_t> build_icmp_echo_request(
        std::uint16_t identifier,
        std::uint16_t sequence,
        const void* payload = nullptr,
        std::size_t payload_len = 0
    ) noexcept {
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

    static int parse_icmp_echo_reply(
        const void* data,
        std::size_t length,
        std::uint16_t expected_id
    ) noexcept {
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

private:
    static int native_protocol(RawProtocol proto, AddressFamily family) noexcept {
    #ifdef OS_WINDOWS
        switch (proto) {
            case RawProtocol::ICMP:
                return (family == AddressFamily::IPv6) ? IPPROTO_ICMPV6 : IPPROTO_ICMP;
            case RawProtocol::ICMPv6:
                return IPPROTO_ICMPV6;
            case RawProtocol::RAW:
                return IPPROTO_RAW;
            default:
                return IPPROTO_ICMP;
        }
    #else
        return -1;
    #endif
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_RAW_SOCKET_HPP