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
    static bool ensure_initialized() noexcept {
    #ifdef OS_WINDOWS
        static const bool ready = [] {
            WSADATA data;
            return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
        }();
        return ready;
    #else
        return true;
    #endif
    }

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
    ) noexcept {
        ResolveOutcome outcome;
        ensure_initialized();
        struct addrinfo hints;
        struct addrinfo* result = nullptr;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family   = static_cast<int>(family);
        hints.ai_socktype = socktype;
        hints.ai_flags    = AI_ADDRCONFIG;
        int status = ::getaddrinfo(hostname.c_str(), nullptr, &hints, &result);

        if (status != 0) {
            outcome.ok = false;
            outcome.native_code = status;
            return outcome;
        }

        for (struct addrinfo* entry = result; entry != nullptr; entry = entry->ai_next) {
            char text[INET6_ADDRSTRLEN];
            text[0] = '\0';

            if (entry->ai_family == AF_INET) {
                auto* v4 = reinterpret_cast<struct sockaddr_in*>(entry->ai_addr);
                ::inet_ntop(AF_INET, &v4->sin_addr, text, INET_ADDRSTRLEN);
                outcome.addresses.emplace_back(text, AddressFamily::IPv4, ntohs(v4->sin_port));
            } else if (entry->ai_family == AF_INET6) {
                auto* v6 = reinterpret_cast<struct sockaddr_in6*>(entry->ai_addr);
                ::inet_ntop(AF_INET6, &v6->sin6_addr, text, INET6_ADDRSTRLEN);
                outcome.addresses.emplace_back(text, AddressFamily::IPv6, ntohs(v6->sin6_port));
            }
        }

        ::freeaddrinfo(result);
        outcome.ok = !outcome.addresses.empty();
        return outcome;
    }

    static ResolveOutcome reverse(const std::string& ip_address) noexcept {
        ResolveOutcome outcome;
        ensure_initialized();
        struct sockaddr_storage storage;
        std::memset(&storage, 0, sizeof(storage));
        socklen_type addr_len = 0;
        AddressFamily family  = AddressFamily::IPv4;
        auto* v4 = reinterpret_cast<struct sockaddr_in*>(&storage);

        if (::inet_pton(AF_INET, ip_address.c_str(), &v4->sin_addr) == 1) {
            v4->sin_family = AF_INET;
            addr_len = static_cast<socklen_type>(sizeof(struct sockaddr_in));
            family   = AddressFamily::IPv4;
        } else {
            auto* v6 = reinterpret_cast<struct sockaddr_in6*>(&storage);

            if (::inet_pton(AF_INET6, ip_address.c_str(), &v6->sin6_addr) == 1) {
                v6->sin6_family = AF_INET6;
                addr_len = static_cast<socklen_type>(sizeof(struct sockaddr_in6));
                family   = AddressFamily::IPv6;
            } else {
                outcome.ok = false;
                outcome.native_code = 0;   
                return outcome;
            }
        }

        char host[NI_MAXHOST];
        host[0] = '\0';

        int status = ::getnameinfo(
            reinterpret_cast<struct sockaddr*>(&storage),
            addr_len,
            host,
            NI_MAXHOST,
            nullptr,
            0,
            NI_NAMEREQD
        );

        if (status != 0) {
            outcome.ok = false;
            outcome.native_code = status;
            return outcome;
        }

        outcome.ok = true;
        outcome.canonical_name = host;
        outcome.addresses.emplace_back(ip_address, family, 0);
        return outcome;
    }

public:
    static SocketError to_error(const ResolveOutcome& outcome, const std::string& operation) noexcept {
    #ifdef OS_WINDOWS
        return WinsockErrorConverter::convert(outcome.native_code, operation);
    #else
        return PosixErrorConverter::convert_gai(outcome.native_code, operation);
    #endif
    }
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_RESOLVER_HPP