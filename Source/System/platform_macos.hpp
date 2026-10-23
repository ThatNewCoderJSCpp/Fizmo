#ifndef FIZMO_SYSTEM_PLATFORM_MACOS_HPP
#define FIZMO_SYSTEM_PLATFORM_MACOS_HPP

#include "common.hpp"

#if defined(OS_MACOS)

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <mach/vm_statistics.h>
#include <libproc.h>

namespace fizmo {
namespace system {
namespace detail {
namespace mac {

inline std::string sysctl_string(const char* name) {
    std::size_t size = 0;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0) return {};
    std::string out(size, '\0');
    if (::sysctlbyname(name, &out[0], &size, nullptr, 0) != 0) return {};
    while (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

inline std::optional<std::uint64_t> sysctl_u64(const char* name) {
    std::uint64_t v64 = 0;
    std::size_t size = sizeof(v64);
    if (::sysctlbyname(name, &v64, &size, nullptr, 0) != 0) return std::nullopt;
    if (size == sizeof(std::uint32_t)) {
        std::uint32_t v32 = 0;
        std::memcpy(&v32, &v64, sizeof(v32));
        return v32;
    }
    if (size != sizeof(std::uint64_t)) return std::nullopt;
    return v64;
}

inline std::uint64_t page_size() {
    vm_size_t size = 0;
    if (::host_page_size(::mach_host_self(), &size) != KERN_SUCCESS || size == 0) return 4096;
    return static_cast<std::uint64_t>(size);
}

} // namespace mac
} // namespace detail
} // namespace system
} // namespace fizmo

#endif // OS_MACOS
#endif // FIZMO_SYSTEM_PLATFORM_MACOS_HPP
