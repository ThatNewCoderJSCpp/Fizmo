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

inline bool read_text(const std::string& p, std::string& out, std::size_t limit = 8u << 20) {
    out.clear();
    const int fd = ::open(full(p).c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    char buf[8192];

    for (;;) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n < 0) { ::close(fd); return false; }
        if (n == 0) break;
        out.append(buf, static_cast<std::size_t>(n));
        if (out.size() >= limit) break;
    }

    ::close(fd);
    return true;
}

inline std::string read_line(const std::string& p) {
    std::string s;
    if (!read_text(p, s, 4096)) return {};
    const std::size_t nl = s.find('\n');
    if (nl != std::string::npos) s.resize(nl);
    return trim(s);
}

inline std::optional<std::uint64_t> read_u64(const std::string& p) { return parse_u64(read_line(p)); }
inline std::optional<std::int64_t> read_i64(const std::string& p) { return parse_i64(read_line(p)); }
inline std::optional<std::uint64_t> read_hex(const std::string& p) { return parse_u64(read_line(p), 16); }

inline bool exists(const std::string& p) {
    struct stat st;
    return ::stat(full(p).c_str(), &st) == 0;
}

inline std::vector<std::string> list(const std::string& p) {
    std::vector<std::string> out;
    DIR* d = ::opendir(full(p).c_str());
    if (!d) return out;

    while (dirent* e = ::readdir(d)) {
        const char* n = e->d_name;
        if (n[0] == '.' && (n[1] == 0 || (n[1] == '.' && n[2] == 0))) continue;
        out.emplace_back(n);
    }

    ::closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

inline std::string base_name(const std::string& p) {
    const std::size_t s = p.find_last_of('/');
    return s == std::string::npos ? p : p.substr(s + 1);
}

inline std::string resolve(const std::string& p) {
    char buf[PATH_MAX];
    if (!::realpath(full(p).c_str(), buf)) return {};
    std::string r = buf;
    const std::string& prefix = root();
    if (!prefix.empty() && r.compare(0, prefix.size(), prefix) == 0) r.erase(0, prefix.size());
    return r;
}

inline std::string read_link(const std::string& p) {
    char buf[PATH_MAX];
    const ssize_t n = ::readlink(full(p).c_str(), buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    return std::string(buf, static_cast<std::size_t>(n));
}

inline std::string link_name(const std::string& p) {
    const std::string r = resolve(p);
    return r.empty() ? std::string() : base_name(r);
}

inline std::map<std::string, std::uint64_t> key_values(const std::string& p) {
    std::map<std::string, std::uint64_t> out;
    std::string text;
    if (!read_text(p, text)) return out;

    for (const std::string& line : split_lines(text)) {
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::vector<std::string> parts = split_ws(line.substr(colon + 1));
        if (parts.empty()) continue;
        const auto v = parse_u64(parts[0]);
        if (!v) continue;
        const bool kb = parts.size() > 1 && (parts[1] == "kB" || parts[1] == "KB" || parts[1] == "kb");
        out[trim(line.substr(0, colon))] = kb ? *v * 1024u : *v;
    }

    return out;
}

inline std::optional<std::uint64_t> value_of(const std::map<std::string, std::uint64_t>& m, const char* key) {
    const auto it = m.find(key);
    if (it == m.end()) return std::nullopt;
    return it->second;
}

struct HwmonTemp {
    std::string label;
    double      celsius = 0.0;
};

inline std::vector<HwmonTemp> hwmon_temps(const std::string& dir) {
    std::vector<HwmonTemp> out;

    for (const std::string& f : list(dir)) {
        if (!starts_with(f, "temp") || !ends_with(f, "_input")) continue;
        const auto v = read_i64(dir + "/" + f);
        if (!v || *v <= -273000 || *v == 0) continue;
        const std::string idx = f.substr(4, f.size() - 4 - 6);
        HwmonTemp t;
        t.label   = read_line(dir + "/temp" + idx + "_label");
        if (t.label.empty()) t.label = "temp" + idx;
        t.celsius = static_cast<double>(*v) / 1000.0;
        out.push_back(t);
    }

    return out;
}

inline std::vector<std::string> hwmon_dirs_under(const std::string& device_dir) {
    std::vector<std::string> out;
    for (const std::string& n : list(device_dir + "/hwmon")) if (starts_with(n, "hwmon")) out.push_back(device_dir + "/hwmon/" + n);
    for (const std::string& n : list(device_dir)) if (starts_with(n, "hwmon")) out.push_back(device_dir + "/" + n);
    return out;
}

inline std::vector<std::pair<std::string, std::string>> hwmon_by_name() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const std::string& n : list("/sys/class/hwmon")) {
        const std::string dir = "/sys/class/hwmon/" + n;
        out.emplace_back(read_line(dir + "/name"), dir);
    }
    return out;
}

struct PciName {
    std::string vendor;
    std::string device;
};

inline PciName pci_name(std::uint32_t vendor, std::uint32_t device) {
    PciName out;
    static const char* files[] = { "/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids", "/usr/share/pci.ids", "/var/lib/pciutils/pci.ids" };
    char vtag[8], dtag[8];
    std::snprintf(vtag, sizeof(vtag), "%04x", vendor & 0xFFFFu);
    std::snprintf(dtag, sizeof(dtag), "%04x", device & 0xFFFFu);

    for (const char* f : files) {
        std::string text;
        if (!read_text(f, text, 64u << 20)) continue;
        bool in_vendor = false;
        std::size_t pos = 0;

        while (pos < text.size()) {
            std::size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            const char* line = text.data() + pos;
            const std::size_t len = end - pos;
            pos = end + 1;
            if (len < 6 || line[0] == '#') continue;

            if (line[0] != '\t') {
                if (in_vendor) return out;
                if (std::strncmp(line, vtag, 4) == 0 && line[4] == ' ') {
                    in_vendor = true;
                    out.vendor = trim(std::string(line + 4, len - 4));
                }
                continue;
            }

            if (in_vendor && line[1] != '\t' && len > 6 && std::strncmp(line + 1, dtag, 4) == 0 && line[5] == ' ') {
                out.device = trim(std::string(line + 5, len - 5));
                return out;
            }
        }

        if (!out.vendor.empty()) return out;
    }

    return out;
}

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
