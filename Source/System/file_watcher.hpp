#ifndef FIZMO_SYSTEM_FILE_WATCHER_HPP
#define FIZMO_SYSTEM_FILE_WATCHER_HPP

#include "../Basic/fizmo_defines.hpp"
#include "common.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(OS_LINUX)
#include <cerrno>
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace fizmo {
namespace system {

enum class FileAction : std::uint8_t { Added = 0, Modified, Removed };

struct FileChange {
    std::filesystem::path path;
    FileAction            action = FileAction::Modified;
};

enum class WatchBackend : std::uint8_t { Native = 0, Polling };

class FileWatcher {
public:
    using Callback = std::function<void(const FileChange&)>;
    using WatchId  = std::uint64_t;
    using Clock    = std::chrono::steady_clock;

private:
    using Path = std::filesystem::path;

    struct Stamp {
        std::filesystem::file_time_type time{};
        std::uintmax_t                  size = 0;
        bool                            dir  = false;
    };

    struct Watch {
        WatchId                id = 0;
        Path                   target;
        bool                   is_dir    = false;
        bool                   recursive = false;
        bool                   polled    = false;
        bool                   exists    = false;
        Callback               callback;
        std::vector<Path>      dirs;
        std::map<Path, Stamp>  snapshot;
    };

    struct Pending {
        FileAction        action = FileAction::Modified;
        Clock::time_point last;
    };

    std::vector<std::unique_ptr<Watch>> m_watches;
    std::map<Path, Pending>             m_pending;
    WatchId                             m_next_id = 1;
    WatchBackend                        m_backend = WatchBackend::Native;
    std::chrono::milliseconds           m_debounce{ 75 };
    std::chrono::milliseconds           m_poll_interval{ 500 };
    Clock::time_point                   m_last_scan{};

#if defined(OS_LINUX)
    int                                 m_fd = -1;
    std::map<Path, std::pair<int, int>> m_dir_wd;
    std::unordered_map<int, Path>       m_wd_dir;
#elif defined(OS_WINDOWS)
    struct WinDir;
    struct WinState;
    detail::Opaque<WinState> m_win;

#endif

    static Path normal(const Path& p);

    static bool under(const Path& path, const Path& dir);

    static bool matches(const Watch& w, const Path& path);

    void record(const Path& raw, FileAction action);

    static std::map<Path, Stamp> take_snapshot(const Watch& w);

    void diff(Watch& w);

#if defined(OS_LINUX)
    static constexpr std::uint32_t kMask = IN_CLOSE_WRITE | IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF;

    bool add_dir(const Path& dir);

    void release_dir(const Path& dir);

    bool attach_native(Watch& w);

    void detach_native(Watch& w) {
        for (const Path& d : w.dirs) release_dir(d);
        w.dirs.clear();
    }

    void new_subdir(const Path& dir);

    void read_native();

    void open_native() {
        m_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (m_fd < 0) m_backend = WatchBackend::Polling;
    }

    void close_native() {
        if (m_fd >= 0) ::close(m_fd);
        m_fd = -1;
        m_dir_wd.clear();
        m_wd_dir.clear();
    }

#elif defined(OS_WINDOWS)

    static bool issue(WinDir& d);

    static void close_dir(WinDir& d);

    bool add_dir(const Path& dir, bool subtree);

    void release_dir(const Path& dir, bool subtree);

    bool attach_native(Watch& w);

    void detach_native(Watch& w);

    void read_native();

    void open_native();

    void close_native();

#else
    bool attach_native(Watch&) { return false; }
    void detach_native(Watch&) {}
    void read_native() {}
    void open_native() { m_backend = WatchBackend::Polling; }
    void close_native() {}
#endif

public:
    explicit FileWatcher(WatchBackend backend = WatchBackend::Native);

    ~FileWatcher() { close_native(); }

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    WatchBackend backend() const noexcept { return m_backend; }
    void set_debounce(std::chrono::milliseconds d) noexcept { m_debounce = d; }
    void set_poll_interval(std::chrono::milliseconds d) noexcept { m_poll_interval = d; }
    std::size_t watch_count() const noexcept { return m_watches.size(); }

    bool is_polled(WatchId id) const noexcept {
        for (const auto& w : m_watches) if (w->id == id) return w->polled;
        return false;
    }

    WatchId watch(const Path& path, Callback callback, bool recursive = false);

    bool unwatch(WatchId id);

    void clear();

    std::size_t poll();

    std::size_t wait(std::chrono::milliseconds timeout, std::chrono::milliseconds step = std::chrono::milliseconds(10));
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_FILE_WATCHER_HPP
