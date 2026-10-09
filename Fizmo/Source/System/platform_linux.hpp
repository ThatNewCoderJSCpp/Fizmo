#ifndef FIZMO_SYSTEM_PLATFORM_LINUX_HPP
#define FIZMO_SYSTEM_PLATFORM_LINUX_HPP

#include "common.hpp"

#if defined(OS_LINUX)

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <climits>
#include <cstdlib>

namespace fizmo {
namespace system {
namespace detail {
namespace lnx {

inline std::string& root() {
    static std::string r;
    return r;
}

inline std::string full(const std::string& p) { return root() + p; }

bool read_text(const std::string& p, std::string& out, std::size_t limit = 8u << 20);

std::string read_line(const std::string& p);

inline std::optional<std::uint64_t> read_u64(const std::string& p) { return parse_u64(read_line(p)); }
inline std::optional<std::int64_t> read_i64(const std::string& p) { return parse_i64(read_line(p)); }
inline std::optional<std::uint64_t> read_hex(const std::string& p) { return parse_u64(read_line(p), 16); }

inline bool exists(const std::string& p) {
    struct stat st;
    return ::stat(full(p).c_str(), &st) == 0;
}

std::vector<std::string> list(const std::string& p);

inline std::string base_name(const std::string& p) {
    const std::size_t s = p.find_last_of('/');
    return s == std::string::npos ? p : p.substr(s + 1);
}

std::string resolve(const std::string& p);

std::string read_link(const std::string& p);

inline std::string link_name(const std::string& p) {
    const std::string r = resolve(p);
    return r.empty() ? std::string() : base_name(r);
}

std::map<std::string, std::uint64_t> key_values(const std::string& p);

inline std::optional<std::uint64_t> value_of(const std::map<std::string, std::uint64_t>& m, const char* key) {
    const auto it = m.find(key);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

struct HwmonTemp {
    std::string label;
    double      celsius = 0.0;
};

std::vector<HwmonTemp> hwmon_temps(const std::string& dir);

std::vector<std::string> hwmon_dirs_under(const std::string& device_dir);

std::vector<std::pair<std::string, std::string>> hwmon_by_name();

struct PciName {
    std::string vendor;
    std::string device;
};

PciName pci_name(std::uint32_t vendor, std::uint32_t device);

class SharedLibrary {
private:
    void* m_handle = nullptr;

public:
    SharedLibrary() noexcept = default;
    explicit SharedLibrary(const char* name) noexcept { m_handle = ::dlopen(name, RTLD_NOW | RTLD_LOCAL); }
    ~SharedLibrary() { if (m_handle) ::dlclose(m_handle); }
    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;

    bool valid() const noexcept { return m_handle != nullptr; }

    template <typename F>
    F get(const char* name) const noexcept {
        if (!m_handle) return nullptr;
        void* p = ::dlsym(m_handle, name);
        F f = nullptr;
        static_assert(sizeof(f) == sizeof(p), "function pointer size");
        std::memcpy(&f, &p, sizeof(f));
        return f;
    }
};

} // namespace lnx
} // namespace detail
} // namespace system
} // namespace fizmo

#endif // OS_LINUX

#endif // FIZMO_SYSTEM_PLATFORM_LINUX_HPP
