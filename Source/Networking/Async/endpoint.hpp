#ifndef FIZMO_ASYNC_ENDPOINT_HPP
#define FIZMO_ASYNC_ENDPOINT_HPP

#include "platform.hpp"
#include "../Core/addresses.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace fizmo {
namespace networking {
namespace async {

enum class Protocol {
    V4,
    V6
};

class Endpoint {
public:
    Endpoint() noexcept;
    Endpoint(const std::string& address, std::uint16_t port);
    Endpoint(Protocol protocol, std::uint16_t port) noexcept;
    Endpoint(const sockaddr* addr, std::size_t len) noexcept;
    explicit Endpoint(const core::NetworkAddress& address);

    static bool parse(const std::string& address, std::uint16_t port, Endpoint& out) noexcept;
    static bool parse(const std::string& host_and_port, Endpoint& out) noexcept;

    static Endpoint any_v4(std::uint16_t port = 0) noexcept { return Endpoint(Protocol::V4, port); }
    static Endpoint any_v6(std::uint16_t port = 0) noexcept { return Endpoint(Protocol::V6, port); }
    static Endpoint loopback_v4(std::uint16_t port = 0);
    static Endpoint loopback_v6(std::uint16_t port = 0);

public:
    int  family() const noexcept;
    bool is_v4() const noexcept { return family() == AF_INET; }
    bool is_v6() const noexcept { return family() == AF_INET6; }
    bool valid() const noexcept { return m_size != 0; }
    Protocol protocol() const noexcept { return is_v6() ? Protocol::V6 : Protocol::V4; }

    std::string   address() const;
    std::uint16_t port() const noexcept;
    void          set_port(std::uint16_t port) noexcept;
    std::string   to_string() const;

    bool is_loopback() const noexcept;
    bool is_multicast() const noexcept;
    bool is_unspecified() const noexcept;
    bool is_v4_mapped() const noexcept;

    Endpoint to_v4_unmapped() const noexcept;

    core::NetworkAddress to_network_address() const;

    const sockaddr* data() const noexcept { return reinterpret_cast<const sockaddr*>(&m_storage); }
    sockaddr*       data()       noexcept { return reinterpret_cast<sockaddr*>(&m_storage); }
    socklen_type    size() const noexcept { return m_size; }
    socklen_type    capacity() const noexcept { return static_cast<socklen_type>(sizeof(m_storage)); }
    void            resize(socklen_type n) noexcept { m_size = n; }

    std::size_t hash() const noexcept;

    friend bool operator==(const Endpoint& a, const Endpoint& b) noexcept;
    friend bool operator!=(const Endpoint& a, const Endpoint& b) noexcept { return !(a == b); }
    friend bool operator<(const Endpoint& a, const Endpoint& b) noexcept;

private:
    sockaddr_storage m_storage;
    socklen_type     m_size;
};

} // namespace async
} // namespace networking
} // namespace fizmo

namespace std {
template <>
struct hash<fizmo::networking::async::Endpoint> {
    std::size_t operator()(const fizmo::networking::async::Endpoint& e) const noexcept { return e.hash(); }
};
} // namespace std

#endif // FIZMO_ASYNC_ENDPOINT_HPP
