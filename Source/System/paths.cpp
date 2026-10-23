#include "fizmo_library.hpp"
#include "paths.hpp"

#if defined(OS_WINDOWS)
#include <shlobj.h>

namespace fizmo {
namespace system {
namespace paths {
namespace detail {

fs::path known_folder(const KNOWNFOLDERID& id);

} // namespace detail
} // namespace paths
} // namespace system
} // namespace fizmo
#endif

namespace fizmo {
namespace system {
namespace paths {
namespace detail {

fs::path u8(const std::string& s) {
#if defined(__cpp_char8_t)
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
#else
    return fs::u8path(s);
#endif
}

struct AppDirectories {
    std::mutex  lock;
    std::string data;
    std::string cache;
    std::string external;
};

AppDirectories& app_directories() {
    static AppDirectories dirs;
    return dirs;
}

std::string app_directory(std::string AppDirectories::* member) {
    AppDirectories& d = app_directories();
    std::lock_guard<std::mutex> g(d.lock);
    return d.*member;
}

fs::path android_temp_dir() {
    const std::string t = env("TMPDIR");
    if (!t.empty()) return u8(t);
    const std::string c = app_directory(&AppDirectories::cache);
    return c.empty() ? fs::path() : u8(c);
}

} // namespace detail
} // namespace paths
} // namespace system
} // namespace fizmo

namespace fizmo {
namespace system {
namespace paths {

fs::path executable_path() {
#if defined(OS_WINDOWS)
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
        if (n == 0) return {};
        if (n < buf.size()) { buf.resize(n); break; }
        buf.resize(buf.size() * 2);
    }
    return fs::path(buf);
#elif defined(OS_LINUX_KERNEL)
    std::error_code ec;
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::path() : p;
#elif defined(OS_MACOS)
    std::uint32_t size = 0;
    ::_NSGetExecutablePath(nullptr, &size);
    if (size == 0) return {};
    std::string buf(size, '\0');
    if (::_NSGetExecutablePath(&buf[0], &size) != 0) return {};
    buf.resize(std::strlen(buf.c_str()));
    char real[PATH_MAX];
    if (::realpath(buf.c_str(), real)) return fs::path(real);
    return fs::path(buf);
#else
    return {};
#endif
}

fs::path home_dir() {
#if defined(OS_WINDOWS)
    fs::path p = detail::known_folder(FOLDERID_Profile);
    if (p.empty()) p = fs::path(detail::env("USERPROFILE"));
    return p;
#elif defined(OS_ANDROID)
    const std::string d = detail::app_directory(&detail::AppDirectories::data);
    if (!d.empty()) return detail::u8(d);
    std::string h = detail::env("HOME");
    if (h.empty()) {
        const passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) h = pw->pw_dir;
    }
    return fs::path(h);
#elif defined(OS_POSIX)
    std::string h = detail::env("HOME");
    if (h.empty()) {
        const passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) h = pw->pw_dir;
    }
    return fs::path(h);
#else
    return fs::path(detail::env("HOME"));
#endif
}

fs::path data_dir(const std::string& app) {
#if defined(OS_WINDOWS)
    return detail::with_app(detail::known_folder(FOLDERID_RoamingAppData), app);
#elif defined(OS_MACOS)
    return detail::with_app(home_dir() / "Library" / "Application Support", app);
#elif defined(OS_ANDROID)
    const std::string d = detail::app_directory(&detail::AppDirectories::data);
    if (!d.empty()) return detail::with_app(detail::u8(d), app);
    const std::string x = detail::env("XDG_DATA_HOME");
    return detail::with_app(x.empty() ? home_dir() / ".local" / "share" : fs::path(x), app);
#else
    const std::string x = detail::env("XDG_DATA_HOME");
    return detail::with_app(x.empty() ? home_dir() / ".local" / "share" : fs::path(x), app);
#endif
}

fs::path config_dir(const std::string& app) {
#if defined(OS_WINDOWS)
    return detail::with_app(detail::known_folder(FOLDERID_RoamingAppData), app);
#elif defined(OS_MACOS)
    return detail::with_app(home_dir() / "Library" / "Application Support", app);
#elif defined(OS_ANDROID)
    const std::string d = detail::app_directory(&detail::AppDirectories::data);
    if (!d.empty()) return detail::with_app(detail::u8(d) / "config", app);
    const std::string x = detail::env("XDG_CONFIG_HOME");
    return detail::with_app(x.empty() ? home_dir() / ".config" : fs::path(x), app);
#else
    const std::string x = detail::env("XDG_CONFIG_HOME");
    return detail::with_app(x.empty() ? home_dir() / ".config" : fs::path(x), app);
#endif
}

fs::path cache_dir(const std::string& app) {
#if defined(OS_WINDOWS)
    return detail::with_app(detail::known_folder(FOLDERID_LocalAppData), app) / "Cache";
#elif defined(OS_MACOS)
    return detail::with_app(home_dir() / "Library" / "Caches", app);
#elif defined(OS_ANDROID)
    const std::string c = detail::app_directory(&detail::AppDirectories::cache);
    if (!c.empty()) return detail::with_app(detail::u8(c), app);
    const std::string x = detail::env("XDG_CACHE_HOME");
    return detail::with_app(x.empty() ? home_dir() / ".cache" : fs::path(x), app);
#else
    const std::string x = detail::env("XDG_CACHE_HOME");
    return detail::with_app(x.empty() ? home_dir() / ".cache" : fs::path(x), app);
#endif
}

fs::path documents_dir() {
#if defined(OS_WINDOWS)
    return detail::known_folder(FOLDERID_Documents);
#elif defined(OS_ANDROID)
    const std::string e = detail::app_directory(&detail::AppDirectories::external);
    if (!e.empty()) return detail::u8(e) / "Documents";
    const std::string d = detail::app_directory(&detail::AppDirectories::data);
    if (!d.empty()) return detail::u8(d) / "Documents";
    const std::string x = detail::env("XDG_DOCUMENTS_DIR");
    return x.empty() ? home_dir() / "Documents" : fs::path(x);
#else
    const std::string x = detail::env("XDG_DOCUMENTS_DIR");
    return x.empty() ? home_dir() / "Documents" : fs::path(x);
#endif
}

void set_app_directories(const std::string& data, const std::string& cache, const std::string& external) {
    {
        detail::AppDirectories& d = detail::app_directories();
        std::lock_guard<std::mutex> g(d.lock);
        d.data = data;
        d.cache = cache;
        d.external = external;
    }
#if defined(OS_ANDROID)
    if (!data.empty()) AssetPaths::instance().add_root(detail::u8(data) / "assets");
#endif // OS_ANDROID
}

std::optional<std::vector<std::uint8_t>> read_bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return std::nullopt;
    const std::streamoff size = f.tellg();
    if (size < 0) return std::nullopt;
    std::vector<std::uint8_t> out(static_cast<std::size_t>(size));
    f.seekg(0);
    if (size > 0 && !f.read(reinterpret_cast<char*>(out.data()), size)) return std::nullopt;
    return out;
}

std::optional<std::string> read_text(const fs::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return std::nullopt;
    const std::streamoff size = f.tellg();
    if (size < 0) return std::nullopt;
    std::string out(static_cast<std::size_t>(size), '\0');
    f.seekg(0);
    if (size > 0 && !f.read(&out[0], size)) return std::nullopt;
    return out;
}

bool write_file(const fs::path& p, const void* data, std::size_t size) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::random_device rd;
    fs::path tmp = p;
    tmp += ".tmp" + std::to_string(rd());

    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        if (size && !f.write(static_cast<const char*>(data), static_cast<std::streamsize>(size))) { f.close(); fs::remove(tmp, ec); return false; }
        f.flush();
        if (!f) { f.close(); fs::remove(tmp, ec); return false; }
    }

#if defined(OS_WINDOWS)
    if (!MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { fs::remove(tmp, ec); return false; }
#else
    fs::rename(tmp, p, ec);
    if (ec) { fs::remove(tmp, ec); return false; }
#endif
    return true;
}

auto AssetPaths::operator=(const AssetPaths& o) -> AssetPaths& {
    if (this == &o) return *this;
    std::vector<fs::path> copy;
    { std::lock_guard<std::mutex> g(o.m_lock); copy = o.m_roots; }
    std::lock_guard<std::mutex> g(m_lock);
    m_roots = std::move(copy);
    return *this;
}

AssetPaths& AssetPaths::instance() {
    static AssetPaths a = [] {
        AssetPaths p;
        p.add_defaults();
        return p;
    }();
    return a;
}

void AssetPaths::add_defaults() {
    const std::string env = detail::env("FIZMO_ASSET_PATH");
#if defined(OS_WINDOWS)
    const char sep = ';';
#else
    const char sep = ':';
#endif
    std::size_t start = 0;
    while (!env.empty() && start <= env.size()) {
        std::size_t end = env.find(sep, start);
        if (end == std::string::npos) end = env.size();
        if (end > start) add_root(detail::u8(env.substr(start, end - start)));
        start = end + 1;
    }
    const fs::path exe = executable_dir();
    add_root(exe / "assets");
    add_root(exe);
    add_root(current_dir() / "assets");
    add_root(current_dir());
#if defined(OS_ANDROID)
    const std::string data = detail::app_directory(&detail::AppDirectories::data);
    if (!data.empty()) add_root(detail::u8(data) / "assets");
#endif // OS_ANDROID
}

void AssetPaths::add_root(const fs::path& root, bool front) {
    std::error_code ec;
    fs::path p = fs::weakly_canonical(root, ec);
    if (ec) p = root;
    std::lock_guard<std::mutex> g(m_lock);
    for (const fs::path& r : m_roots) if (r == p) return;
    if (front) m_roots.insert(m_roots.begin(), p);
    else m_roots.push_back(p);
}

bool AssetPaths::remove_root(const fs::path& root) {
    std::error_code ec;
    fs::path p = fs::weakly_canonical(root, ec);
    if (ec) p = root;
    std::lock_guard<std::mutex> g(m_lock);
    for (auto it = m_roots.begin(); it != m_roots.end(); ++it) if (*it == p) { m_roots.erase(it); return true; }
    return false;
}

auto AssetPaths::resolve(const fs::path& relative) const -> std::optional<fs::path> {
    std::error_code ec;
    if (relative.is_absolute()) return fs::exists(relative, ec) ? std::optional<fs::path>(relative) : std::nullopt;
    for (const fs::path& r : roots()) {
        const fs::path candidate = r / relative;
        if (fs::exists(candidate, ec)) return candidate;
    }
    return std::nullopt;
}

} // namespace paths
} // namespace system
} // namespace fizmo

namespace fizmo {
namespace system {
namespace paths {
namespace detail {

#if defined(OS_WINDOWS)
fs::path known_folder(const KNOWNFOLDERID& id) {
    PWSTR p = nullptr;
    fs::path out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)) && p) out = fs::path(p);
    if (p) CoTaskMemFree(p);
    return out;
}
#endif

} // namespace detail
} // namespace paths
} // namespace system
} // namespace fizmo
