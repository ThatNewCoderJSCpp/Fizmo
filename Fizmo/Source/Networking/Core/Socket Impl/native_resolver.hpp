#ifndef FIZMO_NATIVE_RESOLVER_HPP
#define FIZMO_NATIVE_RESOLVER_HPP

#include "native_impl.hpp"
#include "../Error Conversion/native_errors.hpp"
#include <string>
#include <vector>
#include <cstring>

#ifdef OS_LINUX
#include <netdb.h>
#endif

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

struct ResolvedAddress {
    std::string   address;
    AddressFamily family;
    std::uint16_t port;

    ResolvedAddress(const std::string& a, AddressFamily f, std::uint16_t p = 0) : address(a), family(f), port(p) {}
};

struct ResolveOutcome {
    bool ok = false;
    int  native_code = 0;
    std::vector<ResolvedAddress> addresses;
    std::string canonical_name;
};

class NameResolver {
public:
    static bool ensure_initialized() noexcept;

public:
    static bool is_ipv4(const std::string& text) noexcept {
        struct in_addr addr;
        return ::inet_pton(AF_INET, text.c_str(), &addr) == 1;
    }

    static bool is_ipv6(const std::string& text) noexcept {
        struct in6_addr addr;
        return ::inet_pton(AF_INET6, text.c_str(), &addr) == 1;
    }

    static bool is_ip_address(const std::string& text) noexcept {
        return is_ipv4(text) || is_ipv6(text);
    }

public:
    static ResolveOutcome forward(
        const std::string& hostname,
        AddressFamily family,
        int socktype = SOCK_STREAM
    ) noexcept;

    static ResolveOutcome reverse(const std::string& ip_address) noexcept;

public:
    static SocketError to_error(const ResolveOutcome& outcome, const std::string& operation) noexcept;
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_RESOLVER_HPP