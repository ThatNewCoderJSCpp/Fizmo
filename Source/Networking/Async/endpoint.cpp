#include "fizmo_library.hpp"
#include "endpoint.hpp"
#include "socket_ops.hpp"

#include <cstring>
#include <stdexcept>

namespace fizmo {
namespace networking {
namespace async {

Endpoint::Endpoint() noexcept : m_size(0) {
    std::memset(&m_storage, 0, sizeof(m_storage));
}

Endpoint::Endpoint(const std::string& address, std::uint16_t port) : Endpoint() {
    if (!parse(address, port, *this)) throw std::invalid_argument("Endpoint: \"" + address + "\" is not a numeric IP address");
}

Endpoint::Endpoint(Protocol protocol, std::uint16_t port) noexcept : Endpoint() {
    if (protocol == Protocol::V4) {
        auto* a = reinterpret_cast<sockaddr_in*>(&m_storage);
        a->sin_family = AF_INET;
        a->sin_port = htons(port);
        a->sin_addr.s_addr = htonl(INADDR_ANY);
        m_size = sizeof(sockaddr_in);
    } else {
        auto* a = reinterpret_cast<sockaddr_in6*>(&m_storage);
        a->sin6_family = AF_INET6;
        a->sin6_port = htons(port);
        a->sin6_addr = in6addr_any;
        m_size = sizeof(sockaddr_in6);
    }
}

Endpoint::Endpoint(const sockaddr* addr, std::size_t len) noexcept : Endpoint() {
    if (!addr || len == 0 || len > sizeof(m_storage)) return;
    std::memcpy(&m_storage, addr, len);
    m_size = static_cast<socklen_type>(len);
}

Endpoint::Endpoint(const core::NetworkAddress& address) : Endpoint(address.host(), address.port()) {}

bool Endpoint::parse(const std::string& address, std::uint16_t port, Endpoint& out) noexcept {
    detail::ensure_socket_runtime();
    Endpoint e;
    std::string host = address;
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']') host = host.substr(1, host.size() - 2);
    std::string scope;
    const auto pct = host.find('%');

    if (pct != std::string::npos) {
        scope = host.substr(pct + 1);
        host = host.substr(0, pct);
    }

    auto* v4 = reinterpret_cast<sockaddr_in*>(&e.m_storage);

    if (scope.empty() && ::inet_pton(AF_INET, host.c_str(), &v4->sin_addr) == 1) {
        v4->sin_family = AF_INET;
        v4->sin_port = htons(port);
        e.m_size = sizeof(sockaddr_in);
        out = e;
        return true;
    }

    std::memset(&e.m_storage, 0, sizeof(e.m_storage));
    auto* v6 = reinterpret_cast<sockaddr_in6*>(&e.m_storage);

    if (::inet_pton(AF_INET6, host.c_str(), &v6->sin6_addr) == 1) {
        v6->sin6_family = AF_INET6;
        v6->sin6_port = htons(port);

        if (!scope.empty()) {
            const unsigned long id = std::strtoul(scope.c_str(), nullptr, 10);
            v6->sin6_scope_id = id != 0 ? static_cast<decltype(v6->sin6_scope_id)>(id) : static_cast<decltype(v6->sin6_scope_id)>(detail::interface_index(scope));
        }

        e.m_size = sizeof(sockaddr_in6);
        out = e;
        return true;
    }

    return false;
}

bool Endpoint::parse(const std::string& host_and_port, Endpoint& out) noexcept {
    std::string host, port_text;

    if (!host_and_port.empty() && host_and_port.front() == '[') {
        const auto close = host_and_port.find(']');
        if (close == std::string::npos || close + 1 >= host_and_port.size() || host_and_port[close + 1] != ':') return false;
        host = host_and_port.substr(1, close - 1);
        port_text = host_and_port.substr(close + 2);
    } else {
        const auto colon = host_and_port.rfind(':');
        if (colon == std::string::npos) return false;
        if (host_and_port.find(':') != colon) return false;
        host = host_and_port.substr(0, colon);
        port_text = host_and_port.substr(colon + 1);
    }

    if (port_text.empty() || port_text.size() > 5) return false;
    unsigned long port = 0;

    for (char c : port_text) {
        if (c < '0' || c > '9') return false;
        port = port * 10 + static_cast<unsigned long>(c - '0');
    }

    if (port > 65535) return false;
    return parse(host, static_cast<std::uint16_t>(port), out);
}

Endpoint Endpoint::loopback_v4(std::uint16_t port) { return Endpoint("127.0.0.1", port); }

Endpoint Endpoint::loopback_v6(std::uint16_t port) { return Endpoint("::1", port); }

int Endpoint::family() const noexcept {
    return m_size == 0 ? AF_UNSPEC : reinterpret_cast<const sockaddr*>(&m_storage)->sa_family;
}

std::string Endpoint::address() const {
    char buf[INET6_ADDRSTRLEN + 1] = {};

    if (is_v4()) {
        auto* a = reinterpret_cast<const sockaddr_in*>(&m_storage);
        ::inet_ntop(AF_INET, const_cast<in_addr*>(&a->sin_addr), buf, sizeof(buf));
        return buf;
    }

    if (is_v6()) {
        auto* a = reinterpret_cast<const sockaddr_in6*>(&m_storage);
        ::inet_ntop(AF_INET6, const_cast<in6_addr*>(&a->sin6_addr), buf, sizeof(buf));
        std::string s = buf;
        if (a->sin6_scope_id != 0) s += "%" + std::to_string(a->sin6_scope_id);
        return s;
    }

    return {};
}

std::uint16_t Endpoint::port() const noexcept {
    if (is_v4()) return ntohs(reinterpret_cast<const sockaddr_in*>(&m_storage)->sin_port);
    if (is_v6()) return ntohs(reinterpret_cast<const sockaddr_in6*>(&m_storage)->sin6_port);
    return 0;
}

void Endpoint::set_port(std::uint16_t port) noexcept {
    if (is_v4()) reinterpret_cast<sockaddr_in*>(&m_storage)->sin_port = htons(port);
    else if (is_v6()) reinterpret_cast<sockaddr_in6*>(&m_storage)->sin6_port = htons(port);
}

std::string Endpoint::to_string() const {
    if (is_v6()) return "[" + address() + "]:" + std::to_string(port());
    if (is_v4()) return address() + ":" + std::to_string(port());
    return "<unspecified>";
}

bool Endpoint::is_loopback() const noexcept {
    if (is_v4()) return (ntohl(reinterpret_cast<const sockaddr_in*>(&m_storage)->sin_addr.s_addr) >> 24) == 127;

    if (is_v6()) {
        const auto& a = reinterpret_cast<const sockaddr_in6*>(&m_storage)->sin6_addr;
        static const unsigned char lo[16] = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 };
        if (std::memcmp(&a, lo, 16) == 0) return true;
        return is_v4_mapped() && to_v4_unmapped().is_loopback();
    }

    return false;
}

bool Endpoint::is_multicast() const noexcept {
    if (is_v4()) return (ntohl(reinterpret_cast<const sockaddr_in*>(&m_storage)->sin_addr.s_addr) >> 28) == 0xE;
    if (is_v6()) return reinterpret_cast<const unsigned char*>(&reinterpret_cast<const sockaddr_in6*>(&m_storage)->sin6_addr)[0] == 0xFF;
    return false;
}

bool Endpoint::is_unspecified() const noexcept {
    if (is_v4()) return reinterpret_cast<const sockaddr_in*>(&m_storage)->sin_addr.s_addr == 0;

    if (is_v6()) {
        static const unsigned char zero[16] = {};
        return std::memcmp(&reinterpret_cast<const sockaddr_in6*>(&m_storage)->sin6_addr, zero, 16) == 0;
    }

    return true;
}

bool Endpoint::is_v4_mapped() const noexcept {
    if (!is_v6()) return false;
    const auto* b = reinterpret_cast<const unsigned char*>(&reinterpret_cast<const sockaddr_in6*>(&m_storage)->sin6_addr);
    for (int i = 0; i < 10; ++i) if (b[i] != 0) return false;
    return b[10] == 0xFF && b[11] == 0xFF;
}

Endpoint Endpoint::to_v4_unmapped() const noexcept {
    if (!is_v4_mapped()) return *this;
    const auto* b = reinterpret_cast<const unsigned char*>(&reinterpret_cast<const sockaddr_in6*>(&m_storage)->sin6_addr);
    Endpoint e(Protocol::V4, port());
    std::memcpy(&reinterpret_cast<sockaddr_in*>(&e.m_storage)->sin_addr, b + 12, 4);
    return e;
}

core::NetworkAddress Endpoint::to_network_address() const {
    return core::NetworkAddress(address(), port(), is_v6() ? core::AddressFamily::IPv6 : core::AddressFamily::IPv4);
}

std::size_t Endpoint::hash() const noexcept {
    std::size_t h = static_cast<std::size_t>(1469598103934665603ull);
    const unsigned char* p = nullptr;
    std::size_t n = 0;

    if (is_v4()) {
        const auto* a = reinterpret_cast<const sockaddr_in*>(&m_storage);
        p = reinterpret_cast<const unsigned char*>(&a->sin_addr);
        n = 4;
        h = (h ^ a->sin_port) * 1099511628211ull;
    } else if (is_v6()) {
        const auto* a = reinterpret_cast<const sockaddr_in6*>(&m_storage);
        p = reinterpret_cast<const unsigned char*>(&a->sin6_addr);
        n = 16;
        h = (h ^ a->sin6_port) * 1099511628211ull;
    }

    for (std::size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

bool operator==(const Endpoint& a, const Endpoint& b) noexcept {
    if (a.family() != b.family()) return false;
    if (a.port() != b.port()) return false;

    if (a.is_v4()) {
        return reinterpret_cast<const sockaddr_in*>(&a.m_storage)->sin_addr.s_addr == reinterpret_cast<const sockaddr_in*>(&b.m_storage)->sin_addr.s_addr;
    }

    if (a.is_v6()) {
        const auto* x = reinterpret_cast<const sockaddr_in6*>(&a.m_storage);
        const auto* y = reinterpret_cast<const sockaddr_in6*>(&b.m_storage);
        return std::memcmp(&x->sin6_addr, &y->sin6_addr, 16) == 0 && x->sin6_scope_id == y->sin6_scope_id;
    }

    return true;
}

bool operator<(const Endpoint& a, const Endpoint& b) noexcept {
    if (a.family() != b.family()) return a.family() < b.family();

    if (a.is_v4()) {
        const auto x = ntohl(reinterpret_cast<const sockaddr_in*>(&a.m_storage)->sin_addr.s_addr);
        const auto y = ntohl(reinterpret_cast<const sockaddr_in*>(&b.m_storage)->sin_addr.s_addr);
        if (x != y) return x < y;
    } else if (a.is_v6()) {
        const int c = std::memcmp(&reinterpret_cast<const sockaddr_in6*>(&a.m_storage)->sin6_addr, &reinterpret_cast<const sockaddr_in6*>(&b.m_storage)->sin6_addr, 16);
        if (c != 0) return c < 0;
    }

    return a.port() < b.port();
}

} // namespace async
} // namespace networking
} // namespace fizmo
