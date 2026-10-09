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
    static unsigned int interface_index(const std::string& iface) noexcept;

private:
    static unsigned int index_from_address(const std::string& address) noexcept;

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
    ) noexcept;

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
    ) noexcept;

    static bool source_membership(
        SocketImplBase* impl,
        AddressFamily family,
        const std::string& group,
        const std::string& source,
        const std::string& iface,
        bool join
    ) noexcept;

public: 
    static bool set_multicast_ttl(SocketImplBase* impl, AddressFamily family, int ttl) noexcept;

    static bool set_multicast_loopback(SocketImplBase* impl, AddressFamily family, bool enable) noexcept;

    static bool set_multicast_interface(
        SocketImplBase* impl,
        AddressFamily family,
        const std::string& iface
    ) noexcept;

public:
    static bool set_unicast_hops(SocketImplBase* impl, AddressFamily family, int hops) noexcept;

    static int get_unicast_hops(SocketImplBase* impl, AddressFamily family) noexcept;

public: 
    static bool header_included_supported(AddressFamily family) noexcept;

    static bool set_header_included(SocketImplBase* impl, AddressFamily family, bool enable) noexcept;
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_IP_OPTIONS_HPP