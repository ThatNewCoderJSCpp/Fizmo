#ifndef FIZMO_RAW_SOCKET_HPP
#define FIZMO_RAW_SOCKET_HPP

#include "socket_base.hpp"
#include "socket_options.hpp"
#include "Socket Impl/native_socket_ops.hpp"
#include "Socket Impl/native_ip_options.hpp"

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
    explicit RawSocket(RawProtocol protocol = RawProtocol::ICMP, AddressFamily family = AddressFamily::IPv4) noexcept;

    RawSocket(const RawSocket&) = delete;
    RawSocket& operator=(const RawSocket&) = delete;

    RawSocket(RawSocket&& other) noexcept : SocketBase(std::move(other)), m_protocol(other.m_protocol) {}

    RawSocket& operator=(RawSocket&& other) noexcept;

    ~RawSocket() override = default;

public:
    RawProtocol protocol() const noexcept { return m_protocol; }

public:
    int send_to(const void* data, std::size_t length, const NetworkAddress& dest) noexcept;

    int receive_from(void* buffer, std::size_t length, NetworkAddress& source) noexcept;

    int peek(void* buffer, std::size_t length, NetworkAddress& source) noexcept;

public:
    bool set_header_included(bool enable) noexcept;

    bool header_included_supported() const noexcept {
        return detail::IPOptions::header_included_supported(m_family);
    }

public:
    bool set_ttl(int ttl) noexcept;

    int get_ttl() const noexcept;

public:
    static std::vector<std::uint8_t> build_icmp_echo_request(
        std::uint16_t identifier,
        std::uint16_t sequence,
        const void* payload = nullptr,
        std::size_t payload_len = 0
    ) noexcept;

    static int parse_icmp_echo_reply(
        const void* data,
        std::size_t length,
        std::uint16_t expected_id
    ) noexcept;

private:
    static int native_protocol(RawProtocol proto, AddressFamily family) noexcept;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_RAW_SOCKET_HPP