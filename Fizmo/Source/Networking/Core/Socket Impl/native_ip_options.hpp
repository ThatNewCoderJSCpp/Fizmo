#ifndef FIZMO_NATIVE_IP_OPTIONS_HPP
#define FIZMO_NATIVE_IP_OPTIONS_HPP

#include "native_impl.hpp"
#include <string>
#include <cstring>

#ifdef OS_WINDOWS
    #include <iphlpapi.h>
    #pragma comment(lib, "iphlpapi.lib")
#else
    #include <net/if.h>
    #include <ifaddrs.h>
#endif

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

class IPOptions {
private:
    static bool ok(int result) noexcept { return result == 0; }

    template <typename T>
    static bool set(SocketImplBase* impl, int level, int option, const T& value) noexcept {
        if (!impl) return false;
        return ok(impl->set_option(level, option, &value, static_cast<int>(sizeof(T))));
    }

    template <typename T>
    static bool get(SocketImplBase* impl, int level, int option, T& value) noexcept {
        if (!impl) return false;
        int length = static_cast<int>(sizeof(T));
        return ok(impl->get_option(level, option, &value, &length));
    }

    static bool is_wildcard(const std::string& iface) noexcept {
        return iface.empty() || iface == "0.0.0.0" || iface == "::";
    }

public:
    static unsigned int interface_index(const std::string& iface) noexcept {
        if (is_wildcard(iface)) return 0;
        unsigned int index = ::if_nametoindex(iface.c_str());
        if (index != 0) return index;
        return index_from_address(iface);
    }

private:
    static unsigned int index_from_address(const std::string& address) noexcept {
    #ifdef OS_WINDOWS
        ULONG size = 15000;
        std::unique_ptr<std::uint8_t[]> storage;
        PIP_ADAPTER_ADDRESSES adapters = nullptr;
        ULONG status = ERROR_BUFFER_OVERFLOW;

        for (int attempt = 0; attempt < 3 && status == ERROR_BUFFER_OVERFLOW; ++attempt) {
            storage.reset(new (std::nothrow) std::uint8_t[size]);
            if (!storage) return 0;
            adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(storage.get());

            status = ::GetAdaptersAddresses(
                AF_UNSPEC,
                GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                nullptr,
                adapters,
                &size
            );
        }

        if (status != NO_ERROR || !adapters) return 0;

        for (PIP_ADAPTER_ADDRESSES a = adapters; a != nullptr; a = a->Next) {
            for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u != nullptr; u = u->Next) {
                char text[INET6_ADDRSTRLEN];
                text[0] = '\0';
                auto* sa = u->Address.lpSockaddr;

                if (sa->sa_family == AF_INET) {
                    auto* v4 = reinterpret_cast<struct sockaddr_in*>(sa);
                    ::inet_ntop(AF_INET, &v4->sin_addr, text, INET_ADDRSTRLEN);
                    if (address == text) return a->IfIndex;
                } else if (sa->sa_family == AF_INET6) {
                    auto* v6 = reinterpret_cast<struct sockaddr_in6*>(sa);
                    ::inet_ntop(AF_INET6, &v6->sin6_addr, text, INET6_ADDRSTRLEN);
                    if (address == text) return a->Ipv6IfIndex;
                }
            }
        }

        return 0;
    #else
        struct ifaddrs* list = nullptr;
        if (::getifaddrs(&list) != 0 || !list) return 0;
        unsigned int found = 0;

        for (struct ifaddrs* entry = list; entry != nullptr; entry = entry->ifa_next) {
            if (!entry->ifa_addr || !entry->ifa_name) continue;
            char text[INET6_ADDRSTRLEN];
            text[0] = '\0';

            if (entry->ifa_addr->sa_family == AF_INET) {
                auto* v4 = reinterpret_cast<struct sockaddr_in*>(entry->ifa_addr);
                ::inet_ntop(AF_INET, &v4->sin_addr, text, INET_ADDRSTRLEN);
            } else if (entry->ifa_addr->sa_family == AF_INET6) {
                auto* v6 = reinterpret_cast<struct sockaddr_in6*>(entry->ifa_addr);
                ::inet_ntop(AF_INET6, &v6->sin6_addr, text, INET6_ADDRSTRLEN);
            } else {
                continue;
            }

            if (address == text) {
                found = ::if_nametoindex(entry->ifa_name);
                break;
            }
        }

        ::freeifaddrs(list);
        return found;
    #endif
    }

public: 
    static bool join_group(
        SocketImplBase* impl,
        AddressFamily family,
        const std::string& group,
        const std::string& iface
    ) noexcept {
        return membership(impl, family, group, iface, true);
    }

    static bool leave_group(
        SocketImplBase* impl,
        AddressFamily family,
        const std::string& group,
        const std::string& iface
    ) noexcept {
        return membership(impl, family, group, iface, false);
    }

private:
    static bool membership(
        SocketImplBase* impl,
        AddressFamily family,
        const std::string& group,
        const std::string& iface,
        bool join
    ) noexcept {
        if (!impl) return false;

        if (family == AddressFamily::IPv6) {
            struct ipv6_mreq request;
            std::memset(&request, 0, sizeof(request));
            if (::inet_pton(AF_INET6, group.c_str(), &request.ipv6mr_multiaddr) != 1) return false;
            request.ipv6mr_interface = interface_index(iface);
            return set(impl, IPPROTO_IPV6, join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP, request);
        }

        struct ip_mreq request;
        std::memset(&request, 0, sizeof(request));
        if (::inet_pton(AF_INET, group.c_str(), &request.imr_multiaddr) != 1) return false;

        if (is_wildcard(iface)) {
            request.imr_interface.s_addr = htonl(INADDR_ANY);
        } else if (::inet_pton(AF_INET, iface.c_str(), &request.imr_interface) != 1) {
            request.imr_interface.s_addr = htonl(INADDR_ANY);
        }

        return set(impl, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, request);
    }

public: 
    static bool join_source_group(
        SocketImplBase* impl,
        AddressFamily family,
        const std::string& group,
        const std::string& source,
        const std::string& iface
    ) noexcept {
        return source_membership(impl, family, group, source, iface, true);
    }

    static bool leave_source_group(
        SocketImplBase* impl,
        AddressFamily family,
        const std::string& group,
        const std::string& source,
        const std::string& iface
    ) noexcept {
        return source_membership(impl, family, group, source, iface, false);
    }

private:
    static bool fill_group_address(
        AddressFamily family,
        const std::string& text,
        struct sockaddr_storage& storage
    ) noexcept {
        std::memset(&storage, 0, sizeof(storage));

        if (family == AddressFamily::IPv6) {
            auto* v6 = reinterpret_cast<struct sockaddr_in6*>(&storage);
            v6->sin6_family = AF_INET6;
            return ::inet_pton(AF_INET6, text.c_str(), &v6->sin6_addr) == 1;
        }

        auto* v4 = reinterpret_cast<struct sockaddr_in*>(&storage);
        v4->sin_family = AF_INET;
        return ::inet_pton(AF_INET, text.c_str(), &v4->sin_addr) == 1;
    }

    static bool source_membership(
        SocketImplBase* impl,
        AddressFamily family,
        const std::string& group,
        const std::string& source,
        const std::string& iface,
        bool join
    ) noexcept {
        if (!impl) return false;
        struct group_source_req request;
        std::memset(&request, 0, sizeof(request));
        request.gsr_interface = interface_index(iface);
        struct sockaddr_storage group_storage;
        struct sockaddr_storage source_storage;
        if (!fill_group_address(family, group, group_storage))   return false;
        if (!fill_group_address(family, source, source_storage)) return false;
        std::memcpy(&request.gsr_group,  &group_storage,  sizeof(group_storage));
        std::memcpy(&request.gsr_source, &source_storage, sizeof(source_storage));
        const int level  = (family == AddressFamily::IPv6) ? IPPROTO_IPV6 : IPPROTO_IP;
        const int option = join ? MCAST_JOIN_SOURCE_GROUP : MCAST_LEAVE_SOURCE_GROUP;
        return set(impl, level, option, request);
    }

public: 
    static bool set_multicast_ttl(SocketImplBase* impl, AddressFamily family, int ttl) noexcept {
        if (family == AddressFamily::IPv6) { return set(impl, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, ttl); }
        return set(impl, IPPROTO_IP, IP_MULTICAST_TTL, ttl);
    }

    static bool set_multicast_loopback(SocketImplBase* impl, AddressFamily family, bool enable) noexcept {
        int value = enable ? 1 : 0;
        if (family == AddressFamily::IPv6) { return set(impl, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, value); }
        return set(impl, IPPROTO_IP, IP_MULTICAST_LOOP, value);
    }

    static bool set_multicast_interface(
        SocketImplBase* impl,
        AddressFamily family,
        const std::string& iface
    ) noexcept {
        if (family == AddressFamily::IPv6) {
            unsigned int index = interface_index(iface);
            if (index == 0 && !is_wildcard(iface)) return false;
            return set(impl, IPPROTO_IPV6, IPV6_MULTICAST_IF, index);
        }

        struct in_addr addr;
        std::memset(&addr, 0, sizeof(addr));

        if (is_wildcard(iface)) {
            addr.s_addr = htonl(INADDR_ANY);
        } else if (::inet_pton(AF_INET, iface.c_str(), &addr) != 1) {
            return false;
        }

        return set(impl, IPPROTO_IP, IP_MULTICAST_IF, addr);
    }

public:
    static bool set_unicast_hops(SocketImplBase* impl, AddressFamily family, int hops) noexcept {
        if (family == AddressFamily::IPv6) { return set(impl, IPPROTO_IPV6, IPV6_UNICAST_HOPS, hops); }
        return set(impl, IPPROTO_IP, IP_TTL, hops);
    }

    static int get_unicast_hops(SocketImplBase* impl, AddressFamily family) noexcept {
        int hops = -1;
        const int level  = (family == AddressFamily::IPv6) ? IPPROTO_IPV6 : IPPROTO_IP;
        const int option = (family == AddressFamily::IPv6) ? IPV6_UNICAST_HOPS : IP_TTL;
        if (!get(impl, level, option, hops)) return -1;
        return hops;
    }

public: 
    static bool header_included_supported(AddressFamily family) noexcept {
        if (family != AddressFamily::IPv6) return true;
    #ifdef IPV6_HDRINCL
        return true;
    #else
        return false;   
    #endif
    }

    static bool set_header_included(SocketImplBase* impl, AddressFamily family, bool enable) noexcept {
        int value = enable ? 1 : 0;

        if (family == AddressFamily::IPv6) {
        #ifdef IPV6_HDRINCL
            return set(impl, IPPROTO_IPV6, IPV6_HDRINCL, value);
        #else
            return false;
        #endif
        }

        return set(impl, IPPROTO_IP, IP_HDRINCL, value);
    }
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_IP_OPTIONS_HPP