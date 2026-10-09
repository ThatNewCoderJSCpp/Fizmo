#ifndef FIZMO_SYSTEM_PATHS_HPP
#define FIZMO_SYSTEM_PATHS_HPP

#include "../Basic/fizmo_defines.hpp"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#if defined(OS_LINUX)
#include <pwd.h>
#include <unistd.h>
#endif

#if defined(OS_WINDOWS)
#include <shlobj.h>
#endif

namespace fizmo {
namespace system {
namespace paths {

namespace fs = std::filesystem;

namespace detail {

 fs::path u8(const std::string& s);

inline std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

#if defined(OS_WINDOWS)
inline fs::path known_folder(const KNOWNFOLDERID& id) {
    PWSTR p = nullptr;
    fs::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)) && p) out = fs::path(p);
    if (p) CoTaskMemFree(p);
    return out;
}
#endif

inline fs::path with_app(fs::path base, const std::string& app) {
    if (base.empty()) return base;
    if (!app.empty()) base /= u8(app);
    return base;
}

} // namespace detail

 fs::path executable_path();

inline fs::path executable_dir() {
    const fs::path p = executable_path();
    return p.empty() ? fs::current_path() : p.parent_path();
}

inline fs::path current_dir() {
    std::error_code ec;
    fs::path p = fs::current_path(ec);
    return ec ? fs::path(".") : p;
}

 fs::path home_dir();

inline fs::path temp_dir() {
    std::error_code ec;
    fs::path p = fs::temp_directory_path(ec);
    return ec ? fs::path(".") : p;
}

 fs::path data_dir(const std::string& app = std::string());

 fs::path config_dir(const std::string& app = std::string());

 fs::path cache_dir(const std::string& app = std::string());

 fs::path documents_dir();

inline fs::path ensure_dir(const fs::path& p) {
    std::error_code ec;
    if (!p.empty()) fs::create_directories(p, ec);
    return p;
}

inline std::string to_utf8(const fs::path& p) {
    const auto s = p.u8string();
    return std::string(s.begin(), s.end());
}

inline fs::path from_utf8(const std::string& s) { return detail::u8(s); }

 std::optional<std::vector<std::uint8_t>> read_bytes(const fs::path& p);

 std::optional<std::string> read_text(const fs::path& p);

 bool write_file(const fs::path& p, const void* data, std::size_t size);

inline bool write_file(const fs::path& p, const std::string& text) { return write_file(p, text.data(), text.size()); }
inline bool write_file(const fs::path& p, const std::vector<std::uint8_t>& bytes) { return write_file(p, bytes.data(), bytes.size()); }

class AssetPaths {
private:
    mutable std::mutex   m_lock;
    std::vector<fs::path> m_roots;

public:
    AssetPaths() = default;

    static AssetPaths& instance() {
        static AssetPaths a = [] {
            AssetPaths p;
            p.add_defaults();
            return p;
        }();
        return a;
    }

    AssetPaths(const AssetPaths& o) { std::lock_guard<std::mutex> g(o.m_lock); m_roots = o.m_roots; }
    AssetPaths& operator=(const AssetPaths& o);

    void add_defaults();

    void add_root(const fs::path& root, bool front = false);

    bool remove_root(const fs::path& root);

    void clear() { std::lock_guard<std::mutex> g(m_lock); m_roots.clear(); }

    std::vector<fs::path> roots() const { std::lock_guard<std::mutex> g(m_lock); return m_roots; }

    std::optional<fs::path> resolve(const fs::path& relative) const;

    std::optional<fs::path> resolve(const std::string& relative) const { return resolve(detail::u8(relative)); }
    std::optional<fs::path> resolve(const char* relative) const { return resolve(detail::u8(relative ? relative : "")); }

    fs::path resolve_or(const std::string& relative) const {
        auto r = resolve(relative);
        return r ? *r : detail::u8(relative);
    }
};

inline std::optional<fs::path> find_asset(const std::string& relative) { return AssetPaths::instance().resolve(relative); }
inline fs::path asset(const std::string& relative) { return AssetPaths::instance().resolve_or(relative); }

} // namespace paths
} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_PATHS_HPP
