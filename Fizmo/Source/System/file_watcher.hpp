#ifndef FIZMO_SYSTEM_FILE_WATCHER_HPP
#define FIZMO_SYSTEM_FILE_WATCHER_HPP

#include "../Basic/fizmo_defines.hpp"
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
    struct WinDir {
        HANDLE             handle = INVALID_HANDLE_VALUE;
        OVERLAPPED         ov{};
        std::vector<DWORD> buffer;
        bool               subtree = false;
        int                refs    = 0;
        bool               pending = false;
    };
    std::map<std::pair<Path, bool>, std::unique_ptr<WinDir>> m_dirs;
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
    static constexpr DWORD kFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_CREATION;

    static bool issue(WinDir& d) {
        d.ov = OVERLAPPED{};
        d.pending = ReadDirectoryChangesW(d.handle, d.buffer.data(), static_cast<DWORD>(d.buffer.size() * sizeof(DWORD)), d.subtree ? TRUE : FALSE, kFilter, nullptr, &d.ov, nullptr) != FALSE;
        return d.pending;
    }

    static void close_dir(WinDir& d) {
        if (d.handle == INVALID_HANDLE_VALUE) return;
        if (d.pending) {
            CancelIoEx(d.handle, &d.ov);
            DWORD bytes = 0;
            GetOverlappedResult(d.handle, &d.ov, &bytes, TRUE);
        }
        CloseHandle(d.handle);
        d.handle = INVALID_HANDLE_VALUE;
        d.pending = false;
    }

    bool add_dir(const Path& dir, bool subtree) {
        const auto key = std::make_pair(dir, subtree);
        auto it = m_dirs.find(key);
        if (it != m_dirs.end()) { ++it->second->refs; return true; }
        auto d = std::make_unique<WinDir>();
        d->handle = CreateFileW(dir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (d->handle == INVALID_HANDLE_VALUE) return false;
        d->buffer.resize(16384);
        d->subtree = subtree;
        d->refs = 1;
        if (!issue(*d)) { close_dir(*d); return false; }
        m_dirs[key] = std::move(d);
        return true;
    }

    void release_dir(const Path& dir, bool subtree) {
        auto it = m_dirs.find(std::make_pair(dir, subtree));
        if (it == m_dirs.end()) return;
        if (--it->second->refs > 0) return;
        close_dir(*it->second);
        m_dirs.erase(it);
    }

    bool attach_native(Watch& w) {
        const Path dir = w.is_dir ? w.target : w.target.parent_path();
        if (!add_dir(dir, w.is_dir && w.recursive)) return false;
        w.dirs.push_back(dir);
        return true;
    }

    void detach_native(Watch& w) {
        for (const Path& d : w.dirs) release_dir(d, w.is_dir && w.recursive);
        w.dirs.clear();
    }

    void read_native() {
        for (auto& kv : m_dirs) {
            WinDir& d = *kv.second;
            if (!d.pending || !HasOverlappedIoCompleted(&d.ov)) continue;
            DWORD bytes = 0;
            d.pending = false;
            const BOOL ok = GetOverlappedResult(d.handle, &d.ov, &bytes, FALSE);

            if (!ok || bytes == 0) {
                for (auto& w : m_watches) if (w->target == kv.first.first || under(w->target, kv.first.first)) record(w->target, FileAction::Modified);
            } else {
                const char* base = reinterpret_cast<const char*>(d.buffer.data());
                for (DWORD offset = 0;;) {
                    const FILE_NOTIFY_INFORMATION* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(base + offset);
                    const std::wstring name(info->FileName, info->FileNameLength / sizeof(WCHAR));
                    const Path path = kv.first.first / Path(name);
                    switch (info->Action) {
                        case FILE_ACTION_ADDED:
                        case FILE_ACTION_RENAMED_NEW_NAME: record(path, FileAction::Added); break;
                        case FILE_ACTION_REMOVED:
                        case FILE_ACTION_RENAMED_OLD_NAME: record(path, FileAction::Removed); break;
                        default: {
                            std::error_code ec;
                            if (!std::filesystem::is_directory(path, ec)) record(path, FileAction::Modified);
                            break;
                        }
                    }
                    if (info->NextEntryOffset == 0) break;
                    offset += info->NextEntryOffset;
                }
            }

            issue(d);
        }
    }

    void open_native() {}

    void close_native() {
        for (auto& kv : m_dirs) close_dir(*kv.second);
        m_dirs.clear();
    }

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
