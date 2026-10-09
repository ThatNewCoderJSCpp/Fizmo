#ifndef FIZMO_NETWORK_ADDRESS_HPP
#define FIZMO_NETWORK_ADDRESS_HPP

#include "../../Basic/basic_includes.hpp"

namespace fizmo {
namespace networking {
namespace core {

enum class AddressFamily {
    IPv4 = AF_INET,
    IPv6 = AF_INET6,
    Any  = AF_UNSPEC
};

class NetworkAddress {
private:
    std::string m_host;
    std::uint16_t m_port;
    AddressFamily m_family;

    static AddressFamily detect_family(const std::string& host);

public:
    NetworkAddress(const std::string& host = "127.0.0.1", std::uint16_t port = 8080, AddressFamily family = AddressFamily::Any);
    NetworkAddress(const NetworkAddress& other) : m_host(other.m_host), m_port(other.m_port), m_family(other.m_family) {}
    NetworkAddress(NetworkAddress&& other) noexcept : m_host(std::move(other.m_host)), m_port(other.m_port), m_family(other.m_family) {}

    NetworkAddress& operator=(const NetworkAddress& other);

    NetworkAddress& operator=(NetworkAddress&& other) noexcept;

public:
    const std::string& host() const noexcept { return m_host; }
    std::uint16_t port() const noexcept { return m_port; }
    AddressFamily family() const noexcept { return m_family; }
    bool is_ipv4() const noexcept { return m_family == AddressFamily::IPv4; }
    bool is_ipv6() const noexcept { return m_family == AddressFamily::IPv6; }

    std::string to_string() const;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NETWORK_ADDRESS_HPP