#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "native_resolver.hpp"

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

auto NameResolver::ensure_initialized() noexcept -> bool {
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

auto NameResolver::forward(
        const std::string& hostname,
        AddressFamily family,
        int socktype 
) noexcept -> ResolveOutcome {
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

auto NameResolver::reverse(const std::string& ip_address) noexcept -> ResolveOutcome {
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

auto NameResolver::to_error(const ResolveOutcome& outcome, const std::string& operation) noexcept -> SocketError {
    #ifdef OS_WINDOWS
        return WinsockErrorConverter::convert(outcome.native_code, operation);
    #else
        return PosixErrorConverter::convert_gai(outcome.native_code, operation);
    #endif
    }

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo
