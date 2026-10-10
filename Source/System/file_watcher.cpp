#include "fizmo_library.hpp"
#include "file_watcher.hpp"

namespace fizmo {
namespace system {

auto FileWatcher::normal(const Path& p) -> Path {
    std::error_code ec;
    Path out = std::filesystem::weakly_canonical(p, ec);
    if (ec) out = std::filesystem::absolute(p, ec);
    return out.lexically_normal();
}

bool FileWatcher::under(const Path& path, const Path& dir) {
    auto d = dir.begin(), p = path.begin();
    for (; d != dir.end(); ++d, ++p) {
        if (d->empty()) continue;
        if (p == path.end() || *d != *p) return false;
    }
    return p != path.end();
}

bool FileWatcher::matches(const Watch& w, const Path& path) {
    if (!w.is_dir) return path == w.target;
    if (w.recursive) return under(path, w.target);
    return path.parent_path() == w.target;
}

void FileWatcher::record(const Path& raw, FileAction action) {
    const Path path = raw.lexically_normal();
    const auto now = Clock::now();
    auto it = m_pending.find(path);
    if (it == m_pending.end()) { m_pending[path] = Pending{ action, now }; return; }
    FileAction& a = it->second.action;
    it->second.last = now;

    if (a == FileAction::Added && action == FileAction::Removed) { m_pending.erase(it); return; }
    if (a == FileAction::Removed && action == FileAction::Added) { a = FileAction::Modified; return; }
    if (a == FileAction::Added) return;
    a = action == FileAction::Removed ? FileAction::Removed : (a == FileAction::Removed ? FileAction::Removed : FileAction::Modified);
}

auto FileWatcher::take_snapshot(const Watch& w) -> std::map<Path, Stamp> {
    std::map<Path, Stamp> out;
    std::error_code ec;
    auto stamp_of = [&](const std::filesystem::directory_entry& e) {
        Stamp s;
        std::error_code e2;
        s.dir = e.is_directory(e2);
        s.time = e.last_write_time(e2);
        if (!s.dir) s.size = e.file_size(e2);
        out[e.path().lexically_normal()] = s;
    };

    if (!w.is_dir) {
        std::filesystem::directory_entry e(w.target, ec);
        if (!ec && e.exists(ec)) stamp_of(e);
        return out;
    }

    if (w.recursive) {
        for (std::filesystem::recursive_directory_iterator it(w.target, std::filesystem::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) stamp_of(*it);
    } else {
        for (std::filesystem::directory_iterator it(w.target, std::filesystem::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) stamp_of(*it);
    }

    return out;
}

void FileWatcher::diff(Watch& w) {
    std::map<Path, Stamp> now = take_snapshot(w);
    for (const auto& kv : now) {
        const auto old = w.snapshot.find(kv.first);
        if (old == w.snapshot.end()) record(kv.first, FileAction::Added);
        else if (!kv.second.dir && (old->second.time != kv.second.time || old->second.size != kv.second.size)) record(kv.first, FileAction::Modified);
    }
    for (const auto& kv : w.snapshot) if (now.find(kv.first) == now.end()) record(kv.first, FileAction::Removed);
    w.snapshot = std::move(now);
}

#if defined(OS_LINUX)
bool FileWatcher::add_dir(const Path& dir) {
    auto it = m_dir_wd.find(dir);
    if (it != m_dir_wd.end()) { ++it->second.second; return true; }
    const int wd = inotify_add_watch(m_fd, dir.c_str(), kMask);
    if (wd < 0) return false;
    auto existing = m_wd_dir.find(wd);
    if (existing != m_wd_dir.end()) {
        auto e = m_dir_wd.find(existing->second);
        if (e != m_dir_wd.end()) { ++e->second.second; return true; }
    }
    m_dir_wd[dir] = { wd, 1 };
    m_wd_dir[wd] = dir;
    return true;
}
#endif

#if defined(OS_LINUX)
void FileWatcher::release_dir(const Path& dir) {
    auto it = m_dir_wd.find(dir);
    if (it == m_dir_wd.end()) return;
    if (--it->second.second > 0) return;
    inotify_rm_watch(m_fd, it->second.first);
    m_wd_dir.erase(it->second.first);
    m_dir_wd.erase(it);
}
#endif

#if defined(OS_LINUX)
bool FileWatcher::attach_native(Watch& w) {
    if (m_fd < 0) return false;

    if (!w.is_dir) {
        if (!add_dir(w.target.parent_path())) return false;
        w.dirs.push_back(w.target.parent_path());
        return true;
    }

    if (!add_dir(w.target)) return false;
    w.dirs.push_back(w.target);

    if (w.recursive) {
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(w.target, std::filesystem::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code e2;
            if (!it->is_directory(e2) || it->is_symlink(e2)) continue;
            const Path d = it->path().lexically_normal();
            if (add_dir(d)) w.dirs.push_back(d);
        }
    }

    return true;
}
#endif

#if defined(OS_LINUX)
void FileWatcher::new_subdir(const Path& dir) {
    for (auto& w : m_watches) {
        if (!w->is_dir || !w->recursive || !under(dir, w->target)) continue;
        if (std::find(w->dirs.begin(), w->dirs.end(), dir) != w->dirs.end()) continue;
        if (add_dir(dir)) w->dirs.push_back(dir);
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(dir, std::filesystem::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
            record(it->path(), FileAction::Added);
            std::error_code e2;
            if (it->is_directory(e2) && !it->is_symlink(e2)) {
                const Path d = it->path().lexically_normal();
                if (add_dir(d)) w->dirs.push_back(d);
            }
        }
    }
}
#endif

#if defined(OS_LINUX)
void FileWatcher::read_native() {
    if (m_fd < 0) return;
    alignas(inotify_event) char buf[65536];

    for (;;) {
        const ssize_t n = ::read(m_fd, buf, sizeof(buf));
        if (n <= 0) break;

        for (char* p = buf; p < buf + n;) {
            const inotify_event* ev = reinterpret_cast<const inotify_event*>(p);
            p += sizeof(inotify_event) + ev->len;

            if (ev->mask & IN_Q_OVERFLOW) {
                for (auto& w : m_watches) record(w->target, FileAction::Modified);
                continue;
            }

            const auto dir = m_wd_dir.find(ev->wd);
            if (dir == m_wd_dir.end()) continue;

            if (ev->mask & IN_IGNORED) {
                const Path gone = dir->second;
                m_dir_wd.erase(gone);
                m_wd_dir.erase(dir);
                for (auto& w : m_watches) w->dirs.erase(std::remove(w->dirs.begin(), w->dirs.end(), gone), w->dirs.end());
                continue;
            }

            const Path path = ev->len ? (dir->second / ev->name) : dir->second;
            if (ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) { record(path, FileAction::Removed); continue; }
            if ((ev->mask & IN_ISDIR) && (ev->mask & (IN_CREATE | IN_MOVED_TO))) new_subdir(path.lexically_normal());
            if (ev->mask & (IN_CREATE | IN_MOVED_TO)) record(path, FileAction::Added);
            else if (ev->mask & (IN_DELETE | IN_MOVED_FROM)) record(path, FileAction::Removed);
            else if (ev->mask & (IN_CLOSE_WRITE | IN_MODIFY | IN_ATTRIB)) { if (!(ev->mask & IN_ISDIR)) record(path, FileAction::Modified); }
        }
    }
}
#endif

FileWatcher::FileWatcher(WatchBackend backend) : m_backend(backend) {
    const char* force = std::getenv("FIZMO_WATCH_POLLING");
    if (force && *force && *force != '0') m_backend = WatchBackend::Polling;
    if (m_backend == WatchBackend::Native) open_native();
}

auto FileWatcher::watch(const Path& path, Callback callback, bool recursive) -> WatchId {
    auto w = std::make_unique<Watch>();
    w->id = m_next_id++;
    w->target = normal(path);
    std::error_code ec;
    w->is_dir = std::filesystem::is_directory(w->target, ec);
    w->recursive = w->is_dir && recursive;
    w->callback = std::move(callback);
    if (!w->is_dir && !std::filesystem::is_directory(w->target.parent_path(), ec)) return 0;
    w->exists = !w->is_dir && std::filesystem::exists(w->target, ec);
    w->polled = !(m_backend == WatchBackend::Native && attach_native(*w));
    if (w->polled) w->snapshot = take_snapshot(*w);
    const WatchId id = w->id;
    m_watches.push_back(std::move(w));
    return id;
}

bool FileWatcher::unwatch(WatchId id) {
    for (auto it = m_watches.begin(); it != m_watches.end(); ++it) {
        if ((*it)->id != id) continue;
        if (!(*it)->polled) detach_native(**it);
        m_watches.erase(it);
        return true;
    }
    return false;
}

void FileWatcher::clear() {
    for (auto& w : m_watches) if (!w->polled) detach_native(*w);
    m_watches.clear();
    m_pending.clear();
}

std::size_t FileWatcher::poll() {
    const auto now = Clock::now();

    if (m_backend == WatchBackend::Native) read_native();

    if (now - m_last_scan >= m_poll_interval) {
        m_last_scan = now;
        for (auto& w : m_watches) if (w->polled) diff(*w);
    }

    std::vector<std::pair<WatchId, FileChange>> ready;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (now - it->second.last < m_debounce) { ++it; continue; }
        for (const auto& w : m_watches) {
            if (!matches(*w, it->first)) continue;
            FileAction action = it->second.action;
            if (!w->is_dir) {
                std::error_code ec;
                const bool now_exists = std::filesystem::exists(w->target, ec);
                if (action == FileAction::Added && w->exists) action = FileAction::Modified;
                if (action == FileAction::Removed && now_exists) action = FileAction::Modified;
                if (action == FileAction::Modified && !w->exists && now_exists) action = FileAction::Added;
                w->exists = now_exists;
            }
            ready.emplace_back(w->id, FileChange{ it->first, action });
        }
        it = m_pending.erase(it);
    }

    std::size_t fired = 0;
    for (const auto& r : ready) {
        for (const auto& w : m_watches) {
            if (w->id != r.first) continue;
            Callback cb = w->callback;
            if (cb) { cb(r.second); ++fired; }
            break;
        }
    }

    return fired;
}

std::size_t FileWatcher::wait(std::chrono::milliseconds timeout, std::chrono::milliseconds step) {
    const auto end = Clock::now() + timeout;
    std::size_t fired = 0;
    while (Clock::now() < end) {
        fired += poll();
        if (fired) break;
        std::this_thread::sleep_for(step);
    }
    return fired;
}

} // namespace system
} // namespace fizmo

namespace fizmo {
namespace system {

#if defined(OS_WINDOWS)
struct FileWatcher::WinDir {
    HANDLE             handle = INVALID_HANDLE_VALUE;
    OVERLAPPED         ov{};
    std::vector<DWORD> buffer;
    bool               subtree = false;
    int                refs    = 0;
    bool               pending = false;
};

struct FileWatcher::WinState {
    std::map<std::pair<Path, bool>, std::unique_ptr<WinDir>> m_dirs;
    static constexpr DWORD kFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_CREATION;
};
#endif
#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
bool FileWatcher::issue(WinDir& d) {
    d.ov = OVERLAPPED{};
    d.pending = ReadDirectoryChangesW(d.handle, d.buffer.data(), static_cast<DWORD>(d.buffer.size() * sizeof(DWORD)), d.subtree ? TRUE : FALSE, WinState::kFilter, nullptr, &d.ov, nullptr) != FALSE;
    return d.pending;
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void FileWatcher::close_dir(WinDir& d) {
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
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
bool FileWatcher::add_dir(const Path& dir, bool subtree) {
    const auto key = std::make_pair(dir, subtree);
    auto it = m_win.get().m_dirs.find(key);
    if (it != m_win.get().m_dirs.end()) { ++it->second->refs; return true; }
    auto d = std::make_unique<WinDir>();
    d->handle = CreateFileW(dir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (d->handle == INVALID_HANDLE_VALUE) return false;
    d->buffer.resize(16384);
    d->subtree = subtree;
    d->refs = 1;
    if (!issue(*d)) { close_dir(*d); return false; }
    m_win.get().m_dirs[key] = std::move(d);
    return true;
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void FileWatcher::release_dir(const Path& dir, bool subtree) {
    auto it = m_win.get().m_dirs.find(std::make_pair(dir, subtree));
    if (it == m_win.get().m_dirs.end()) return;
    if (--it->second->refs > 0) return;
    close_dir(*it->second);
    m_win.get().m_dirs.erase(it);
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
bool FileWatcher::attach_native(Watch& w) {
    const Path dir = w.is_dir ? w.target : w.target.parent_path();
    if (!add_dir(dir, w.is_dir && w.recursive)) return false;
    w.dirs.push_back(dir);
    return true;
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void FileWatcher::detach_native(Watch& w) {
    for (const Path& d : w.dirs) release_dir(d, w.is_dir && w.recursive);
    w.dirs.clear();
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void FileWatcher::read_native() {
    for (auto& kv : m_win.get().m_dirs) {
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
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void FileWatcher::open_native() {}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void FileWatcher::close_native() {
    for (auto& kv : m_win.get().m_dirs) close_dir(*kv.second);
    m_win.get().m_dirs.clear();
}
#endif

} // namespace system
} // namespace fizmo
