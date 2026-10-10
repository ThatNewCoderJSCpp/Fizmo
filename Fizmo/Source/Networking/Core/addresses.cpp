#include "fizmo_library.hpp"
#include "addresses.hpp"

namespace fizmo {
namespace networking {
namespace core {

auto NetworkAddress::detect_family(const std::string& host) -> AddressFamily {
    if (host.find(':') != std::string::npos) return AddressFamily::IPv6;
    return AddressFamily::IPv4;
}

NetworkAddress::NetworkAddress(const std::string& host, std::uint16_t port, AddressFamily family) : m_host(host), m_port(port), m_family(family == AddressFamily::Any ? detect_family(host) : family) {}

auto NetworkAddress::operator=(const NetworkAddress& other) -> NetworkAddress& {
    if (this != &other) {
        m_host = other.m_host;
        m_port = other.m_port;
        m_family = other.m_family;
    }

    return *this;
}

auto NetworkAddress::operator=(NetworkAddress&& other) noexcept -> NetworkAddress& {
    if (this != &other) {
        m_host = std::move(other.m_host);
        m_port = other.m_port;
        m_family = other.m_family;
    }

    return *this;
}

std::string NetworkAddress::to_string() const {
    if (is_ipv6()) return "[" + m_host + "]:" + std::to_string(m_port);
    return m_host + ":" + std::to_string(m_port);
}

} // namespace core
} // namespace networking
} // namespace fizmo
