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

    static AddressFamily detect_family(const std::string& host) {
        if (host.find(':') != std::string::npos) return AddressFamily::IPv6;
        return AddressFamily::IPv4;
    }

public:
    NetworkAddress(const std::string& host = "127.0.0.1", std::uint16_t port = 8080, AddressFamily family = AddressFamily::Any) : m_host(host), m_port(port), m_family(family == AddressFamily::Any ? detect_family(host) : family) {}
    NetworkAddress(const NetworkAddress& other) : m_host(other.m_host), m_port(other.m_port), m_family(other.m_family) {}
    NetworkAddress(NetworkAddress&& other) noexcept : m_host(std::move(other.m_host)), m_port(other.m_port), m_family(other.m_family) {}

    NetworkAddress& operator=(const NetworkAddress& other) {
        if (this != &other) {
            m_host = other.m_host;
            m_port = other.m_port;
            m_family = other.m_family;
        }

        return *this;
    }

    NetworkAddress& operator=(NetworkAddress&& other) noexcept {
        if (this != &other) {
            m_host = std::move(other.m_host);
            m_port = other.m_port;
            m_family = other.m_family;
        }

        return *this;
    }

public:
    const std::string& host() const noexcept { return m_host; }
    std::uint16_t port() const noexcept { return m_port; }
    AddressFamily family() const noexcept { return m_family; }
    bool is_ipv4() const noexcept { return m_family == AddressFamily::IPv4; }
    bool is_ipv6() const noexcept { return m_family == AddressFamily::IPv6; }

    std::string to_string() const {
        if (is_ipv6()) return "[" + m_host + "]:" + std::to_string(m_port);
        return m_host + ":" + std::to_string(m_port);
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NETWORK_ADDRESS_HPP