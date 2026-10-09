#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "file_watcher.hpp"

namespace fizmo {
namespace system {

auto FileWatcher::normal(const Path& p) -> Path {
        std::error_code ec;
        Path out = std::filesystem::weakly_canonical(p, ec);
        if (ec) out = std::filesystem::absolute(p, ec);
        return out.lexically_normal();
    }

auto FileWatcher::under(const Path& path, const Path& dir) -> bool {
        auto d = dir.begin(), p = path.begin();
        for (; d != dir.end(); ++d, ++p) {
            if (d->empty()) continue;
            if (p == path.end() || *d != *p) return false;
        }
        return p != path.end();
    }

auto FileWatcher::matches(const Watch& w, const Path& path) -> bool {
        if (!w.is_dir) return path == w.target;
        if (w.recursive) return under(path, w.target);
        return path.parent_path() == w.target;
    }

auto FileWatcher::record(const Path& raw, FileAction action) -> void {
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

auto FileWatcher::diff(Watch& w) -> void {
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
auto FileWatcher::add_dir(const Path& dir) -> bool {
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
auto FileWatcher::release_dir(const Path& dir) -> void {
        auto it = m_dir_wd.find(dir);
        if (it == m_dir_wd.end()) return;
        if (--it->second.second > 0) return;
        inotify_rm_watch(m_fd, it->second.first);
        m_wd_dir.erase(it->second.first);
        m_dir_wd.erase(it);
    }
#endif

#if defined(OS_LINUX)
auto FileWatcher::attach_native(Watch& w) -> bool {
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
auto FileWatcher::new_subdir(const Path& dir) -> void {
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
auto FileWatcher::read_native() -> void {
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

auto FileWatcher::unwatch(WatchId id) -> bool {
        for (auto it = m_watches.begin(); it != m_watches.end(); ++it) {
            if ((*it)->id != id) continue;
            if (!(*it)->polled) detach_native(**it);
            m_watches.erase(it);
            return true;
        }
        return false;
    }

auto FileWatcher::clear() -> void {
        for (auto& w : m_watches) if (!w->polled) detach_native(*w);
        m_watches.clear();
        m_pending.clear();
    }

auto FileWatcher::poll() -> std::size_t {
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

auto FileWatcher::wait(std::chrono::milliseconds timeout, std::chrono::milliseconds step) -> std::size_t {
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
