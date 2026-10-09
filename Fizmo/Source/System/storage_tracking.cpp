#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "storage_tracking.hpp"

namespace fizmo {
namespace system {

auto StorageTracking::finish_volume(VolumeSample& v) -> void {
        v.usage_percent = v.total ? 100.0 * static_cast<double>(v.total - std::min(v.total, v.free)) / static_cast<double>(v.total) : 0.0;
    }

#if defined(OS_LINUX)
auto StorageTracking::skip_block(const std::string& n) -> bool {
        return detail::starts_with(n, "loop") || detail::starts_with(n, "ram") || detail::starts_with(n, "zram") || detail::starts_with(n, "dm-") || detail::starts_with(n, "fd");
    }
#endif

#if defined(OS_LINUX)
auto StorageTracking::discover() -> void {
        for (const std::string& n : detail::lnx::list("/sys/block")) {
            if (skip_block(n)) continue;
            const std::string dir = "/sys/block/" + n;
            const auto sectors = detail::lnx::read_u64(dir + "/size");
            if (!sectors || *sectors == 0) continue;
            DiskState d;
            d.name     = n;
            d.capacity = *sectors * 512u;
            d.model    = detail::lnx::read_line(dir + "/device/model");
            if (d.model.empty()) d.model = detail::lnx::read_line(dir + "/device/name");
            if (const auto r = detail::lnx::read_u64(dir + "/queue/rotational")) d.rotational = *r != 0;
            d.removable = detail::lnx::read_u64(dir + "/removable").value_or(0) != 0;

            for (const std::string& h : detail::lnx::hwmon_dirs_under(dir + "/device")) {
                const std::string t = h + "/temp1_input";
                if (detail::lnx::exists(t)) { d.temp_input = t; break; }
            }

            m_disks.push_back(d);
        }

        read_disks(nullptr, 0.0);
        read_process(m_prev_proc_read, m_prev_proc_write);
    }
#endif

#if defined(OS_LINUX)
auto StorageTracking::read_disks(StorageSample* out, double wall) -> void {
        for (DiskState& d : m_disks) {
            const std::vector<std::string> f = detail::split_ws(detail::lnx::read_line("/sys/block/" + d.name + "/stat"));
            if (f.size() < 11) continue;
            auto at = [&](std::size_t i) { return detail::parse_u64(f[i]).value_or(0); };
            const std::uint64_t reads = at(0), rsec = at(2), rticks = at(3), writes = at(4), wsec = at(6), wticks = at(7), io = at(9);

            if (out) {
                DiskSample s;
                s.name       = d.name;
                s.model      = d.model;
                s.rotational = d.rotational;
                s.removable  = d.removable;
                s.capacity   = d.capacity;
                s.total_read_bytes    = rsec * 512u;
                s.total_written_bytes = wsec * 512u;

                if (d.primed && wall > 0.0) {
                    s.read_bytes_per_sec  = detail::per_second(rsec, d.read_sectors, wall) * 512.0;
                    s.write_bytes_per_sec = detail::per_second(wsec, d.write_sectors, wall) * 512.0;
                    s.reads_per_sec       = detail::per_second(reads, d.reads, wall);
                    s.writes_per_sec      = detail::per_second(writes, d.writes, wall);
                    s.busy_percent        = detail::clamp_percent(detail::per_second(io, d.io_ticks, wall) / 10.0);
                    const std::uint64_t ops = (reads >= d.reads ? reads - d.reads : 0) + (writes >= d.writes ? writes - d.writes : 0);
                    const std::uint64_t ticks = (rticks >= d.read_ticks ? rticks - d.read_ticks : 0) + (wticks >= d.write_ticks ? wticks - d.write_ticks : 0);
                    if (ops > 0) s.latency_ms = static_cast<double>(ticks) / static_cast<double>(ops);
                }

                if (!d.temp_input.empty()) if (const auto t = detail::lnx::read_i64(d.temp_input)) if (*t > -273000) s.temperature_c = *t / 1000.0;
                out->read_bytes_per_sec  += s.read_bytes_per_sec;
                out->write_bytes_per_sec += s.write_bytes_per_sec;
                out->disks.push_back(std::move(s));
            }

            d.reads = reads; d.read_sectors = rsec; d.read_ticks = rticks;
            d.writes = writes; d.write_sectors = wsec; d.write_ticks = wticks;
            d.io_ticks = io;
            d.primed = true;
        }
    }
#endif

#if defined(OS_LINUX)
auto StorageTracking::read_process(std::uint64_t& read, std::uint64_t& write) -> void {
        const auto io = detail::lnx::key_values("/proc/self/io");
        if (const auto r = detail::lnx::value_of(io, "read_bytes")) read = *r;
        if (const auto w = detail::lnx::value_of(io, "write_bytes")) write = *w;
    }
#endif

#if defined(OS_LINUX)
auto StorageTracking::unescape_mount(const std::string& s) -> std::string {
        std::string out;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 3 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '7') {
                out.push_back(static_cast<char>((s[i + 1] - '0') * 64 + (s[i + 2] - '0') * 8 + (s[i + 3] - '0')));
                i += 3;
            } else out.push_back(s[i]);
        }
        return out;
    }
#endif

#if defined(OS_LINUX)
auto StorageTracking::read_volumes(StorageSample& out) -> void {
        std::string text;
        if (!detail::lnx::read_text("/proc/self/mounts", text)) return;
        static const std::set<std::string> pseudo = {
            "proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "cgroup", "cgroup2", "securityfs", "pstore", "debugfs", "tracefs",
            "configfs", "fusectl", "mqueue", "hugetlbfs", "bpf", "autofs", "binfmt_misc", "rpc_pipefs", "nsfs", "efivarfs", "ramfs", "selinuxfs"
        };
        std::set<std::string> seen;

        for (const std::string& line : detail::split_lines(text)) {
            const std::vector<std::string> f = detail::split_ws(line);
            if (f.size() < 4) continue;
            const std::string device = unescape_mount(f[0]);
            const std::string path   = unescape_mount(f[1]);
            const std::string& fs    = f[2];
            if (pseudo.count(fs)) continue;
            if (path != "/" && !detail::starts_with(device, "/")) continue;
            if (detail::starts_with(device, "/dev/loop")) continue;
            if (!seen.insert(device).second) continue;
            struct statvfs vs;
            if (::statvfs((detail::lnx::root() + path).c_str(), &vs) != 0 || vs.f_blocks == 0) continue;
            VolumeSample v;
            v.path       = path;
            v.device     = device;
            v.filesystem = fs;
            const std::uint64_t frag = vs.f_frsize ? vs.f_frsize : vs.f_bsize;
            v.total      = static_cast<std::uint64_t>(vs.f_blocks) * frag;
            v.free       = static_cast<std::uint64_t>(vs.f_bfree) * frag;
            v.available  = static_cast<std::uint64_t>(vs.f_bavail) * frag;
            v.read_only  = (vs.f_flag & ST_RDONLY) != 0;
            const std::string base = detail::lnx::base_name(device);
            std::string parent = base;
            while (!parent.empty() && !detail::lnx::exists("/sys/block/" + parent)) {
                if (parent.back() >= '0' && parent.back() <= '9') parent.pop_back();
                else if (parent.back() == 'p' && parent.size() > 1) parent.pop_back();
                else break;
            }
            if (!parent.empty()) v.removable = detail::lnx::read_u64("/sys/block/" + parent + "/removable").value_or(0) != 0;
            finish_volume(v);
            out.volumes.push_back(v);
        }
    }
#endif

#if defined(OS_LINUX)
auto StorageTracking::collect() -> StorageSample {
        StorageSample s;
        const detail::Clock::time_point now = detail::Clock::now();
        const double wall = detail::seconds_between(m_prev_time, now);
        read_disks(&s, wall);
        read_volumes(s);
        std::uint64_t r = m_prev_proc_read, w = m_prev_proc_write;
        read_process(r, w);
        s.process_read_bytes          = r;
        s.process_written_bytes       = w;
        s.process_read_bytes_per_sec  = detail::per_second(r, m_prev_proc_read, wall);
        s.process_write_bytes_per_sec = detail::per_second(w, m_prev_proc_write, wall);
        m_prev_proc_read = r;
        m_prev_proc_write = w;
        m_prev_time = now;
        return s;
    }
#endif

auto StorageTracking::report() const -> std::string {
        const StorageSample s = latest();
        std::string r = "Storage  read " + format_rate(s.read_bytes_per_sec) + ", write " + format_rate(s.write_bytes_per_sec) + "\n";

        for (const DiskSample& d : s.disks) {
            r += "  " + d.name;
            if (!d.model.empty()) r += " (" + d.model + ")";
            if (d.rotational) r += *d.rotational ? " hdd" : " ssd";
            r += "  r " + format_rate(d.read_bytes_per_sec) + "  w " + format_rate(d.write_bytes_per_sec);
            if (d.busy_percent) r += "  busy " + detail::fixed(*d.busy_percent, 1) + "%";
            if (d.latency_ms) r += "  " + detail::fixed(*d.latency_ms, 2) + " ms/op";
            if (d.temperature_c) r += "  " + detail::fixed(*d.temperature_c, 0) + " C";
            r += "\n";
        }

        for (const VolumeSample& v : s.volumes) {
            r += "  " + v.path + (v.label.empty() ? "" : " [" + v.label + "]") + " " + v.filesystem + "  " + format_bytes(v.total - std::min(v.total, v.free)) + " / " + format_bytes(v.total) + " (" + detail::fixed(v.usage_percent, 1) + "%)\n";
        }

        r += "  process  read " + format_rate(s.process_read_bytes_per_sec) + ", write " + format_rate(s.process_write_bytes_per_sec) + "\n";
        return r;
    }

} // namespace system
} // namespace fizmo
