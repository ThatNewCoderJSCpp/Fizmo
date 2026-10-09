#include "fizmo_library.hpp"
#include "storage_tracking.hpp"
#include "platform_windows.hpp"

namespace fizmo {
namespace system {

void StorageTracking::finish_volume(VolumeSample& v) {
    v.usage_percent = v.total ? 100.0 * static_cast<double>(v.total - std::min(v.total, v.free)) / static_cast<double>(v.total) : 0.0;
}

#if defined(OS_LINUX)
bool StorageTracking::skip_block(const std::string& n) {
    return detail::starts_with(n, "loop") || detail::starts_with(n, "ram") || detail::starts_with(n, "zram") || detail::starts_with(n, "dm-") || detail::starts_with(n, "fd");
}
#endif

#if defined(OS_LINUX)
void StorageTracking::discover() {
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
void StorageTracking::read_disks(StorageSample* out, double wall) {
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
void StorageTracking::read_process(std::uint64_t& read, std::uint64_t& write) {
    const auto io = detail::lnx::key_values("/proc/self/io");
    if (const auto r = detail::lnx::value_of(io, "read_bytes")) read = *r;
    if (const auto w = detail::lnx::value_of(io, "write_bytes")) write = *w;
}
#endif

#if defined(OS_LINUX)
std::string StorageTracking::unescape_mount(const std::string& s) {
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
void StorageTracking::read_volumes(StorageSample& out) {
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

std::string StorageTracking::report() const {
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

namespace fizmo {
namespace system {

#if defined(OS_WINDOWS)
struct StorageTracking::WinState {
    std::unique_ptr<detail::win::Pdh> m_pdh;
    detail::win::Pdh::Counter m_read_bytes  = nullptr;
    detail::win::Pdh::Counter m_write_bytes = nullptr;
    detail::win::Pdh::Counter m_reads       = nullptr;
    detail::win::Pdh::Counter m_writes      = nullptr;
    detail::win::Pdh::Counter m_idle        = nullptr;
    detail::win::Pdh::Counter m_latency     = nullptr;
    std::vector<detail::win::Pdh::Item> m_items;
};
#endif
#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void* StorageTracking::open_disk(int index) {
    const std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(index);
    return CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void StorageTracking::probe(int index, DiskInfo& d) {
    d.probed = true;
    const HANDLE h = open_disk(index);
    if (h == INVALID_HANDLE_VALUE) return;
    STORAGE_PROPERTY_QUERY q{};
    q.PropertyId = StorageDeviceProperty;
    q.QueryType  = PropertyStandardQuery;
    std::vector<unsigned char> buf(4096);
    DWORD got = 0;

    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr) && got >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
        const auto* desc = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buf.data());
        auto text_at = [&](DWORD off) -> std::string {
            if (off == 0 || off >= got) return {};
            const char* p = reinterpret_cast<const char*>(buf.data() + off);
            return detail::trim(std::string(p, strnlen(p, got - off)));
        };
        const std::string vendor = text_at(desc->VendorIdOffset);
        const std::string product = text_at(desc->ProductIdOffset);
        d.model = vendor.empty() ? product : (product.empty() ? vendor : vendor + " " + product);
        d.removable = desc->RemovableMedia != FALSE;
    }

    q.PropertyId = StorageDeviceSeekPenaltyProperty;
    DEVICE_SEEK_PENALTY_DESCRIPTOR seek{};
    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), &seek, sizeof(seek), &got, nullptr) && got >= sizeof(seek)) d.rotational = seek.IncursSeekPenalty != FALSE;

    GET_LENGTH_INFORMATION len{};
    if (DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &len, sizeof(len), &got, nullptr)) d.capacity = static_cast<std::uint64_t>(len.Length.QuadPart);
    CloseHandle(h);
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
std::optional<double> StorageTracking::temperature(int index) {
    const HANDLE h = open_disk(index);
    if (h == INVALID_HANDLE_VALUE) return std::nullopt;
    STORAGE_PROPERTY_QUERY q{};
    q.PropertyId = StorageDeviceTemperatureProperty;
    q.QueryType  = PropertyStandardQuery;
    alignas(8) unsigned char buf[512] = {};
    DWORD got = 0;
    std::optional<double> out;

    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), buf, sizeof(buf), &got, nullptr) && got >= sizeof(STORAGE_TEMPERATURE_DATA_DESCRIPTOR)) {
        const auto* t = reinterpret_cast<const STORAGE_TEMPERATURE_DATA_DESCRIPTOR*>(buf);
        if (t->InfoCount > 0) {
            const SHORT c = t->TemperatureInfo[0].Temperature;
            if (c > -50 && c < 200) out = static_cast<double>(c);
        }
    }

    CloseHandle(h);
    return out;
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void StorageTracking::read_process(std::uint64_t& read, std::uint64_t& write) {
    IO_COUNTERS io{};
    if (!GetProcessIoCounters(GetCurrentProcess(), &io)) return;
    read  = io.ReadTransferCount;
    write = io.WriteTransferCount;
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void StorageTracking::read_volumes(StorageSample& out) {
    wchar_t drives[512];
    const DWORD n = GetLogicalDriveStringsW(511, drives);
    if (n == 0 || n > 511) return;
    const UINT old_mode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    for (const wchar_t* p = drives; *p; p += wcslen(p) + 1) {
        const UINT type = GetDriveTypeW(p);
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE) continue;
        ULARGE_INTEGER avail{}, total{}, free{};
        if (!GetDiskFreeSpaceExW(p, &avail, &total, &free) || total.QuadPart == 0) continue;
        VolumeSample v;
        v.path      = detail::win::narrow(p);
        v.device    = v.path;
        v.total     = total.QuadPart;
        v.free      = free.QuadPart;
        v.available = avail.QuadPart;
        v.removable = type == DRIVE_REMOVABLE;
        wchar_t label[MAX_PATH + 1] = {}, fs[MAX_PATH + 1] = {};
        DWORD flags = 0;

        if (GetVolumeInformationW(p, label, MAX_PATH + 1, nullptr, nullptr, &flags, fs, MAX_PATH + 1)) {
            v.label      = detail::win::narrow(label);
            v.filesystem = detail::win::narrow(fs);
            v.read_only  = (flags & FILE_READ_ONLY_VOLUME) != 0;
        }

        finish_volume(v);
        out.volumes.push_back(v);
    }

    SetErrorMode(old_mode);
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void StorageTracking::discover() {
    m_win.get().m_pdh = std::make_unique<detail::win::Pdh>();
    if (m_win.get().m_pdh->valid()) {
        m_win.get().m_read_bytes  = m_win.get().m_pdh->add(L"\\PhysicalDisk(*)\\Disk Read Bytes/sec");
        m_win.get().m_write_bytes = m_win.get().m_pdh->add(L"\\PhysicalDisk(*)\\Disk Write Bytes/sec");
        m_win.get().m_reads       = m_win.get().m_pdh->add(L"\\PhysicalDisk(*)\\Disk Reads/sec");
        m_win.get().m_writes      = m_win.get().m_pdh->add(L"\\PhysicalDisk(*)\\Disk Writes/sec");
        m_win.get().m_idle        = m_win.get().m_pdh->add(L"\\PhysicalDisk(*)\\% Idle Time");
        m_win.get().m_latency     = m_win.get().m_pdh->add(L"\\PhysicalDisk(*)\\Avg. Disk sec/Transfer");
        m_win.get().m_pdh->collect();
    }
    read_process(m_prev_proc_read, m_prev_proc_write);
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
int StorageTracking::disk_index(const std::string& instance) {
    if (instance.empty() || instance[0] < '0' || instance[0] > '9') return -1;
    return std::atoi(instance.c_str());
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
auto StorageTracking::collect() -> StorageSample {
    StorageSample s;
    const detail::Clock::time_point now = detail::Clock::now();
    const double wall = detail::seconds_between(m_prev_time, now);
    std::map<int, DiskSample> disks;

    if (m_win.get().m_pdh && m_win.get().m_pdh->valid() && m_win.get().m_pdh->collect()) {
        auto gather = [&](detail::win::Pdh::Counter c, const std::function<void(DiskSample&, double)>& apply) {
            if (!m_win.get().m_pdh->items(c, m_win.get().m_items)) return;
            for (const auto& it : m_win.get().m_items) {
                const int idx = disk_index(it.name);
                if (idx < 0) continue;
                DiskSample& d = disks[idx];
                if (d.name.empty()) d.name = it.name;
                apply(d, it.value);
            }
        };
        gather(m_win.get().m_read_bytes,  [](DiskSample& d, double v) { d.read_bytes_per_sec = v; });
        gather(m_win.get().m_write_bytes, [](DiskSample& d, double v) { d.write_bytes_per_sec = v; });
        gather(m_win.get().m_reads,       [](DiskSample& d, double v) { d.reads_per_sec = v; });
        gather(m_win.get().m_writes,      [](DiskSample& d, double v) { d.writes_per_sec = v; });
        gather(m_win.get().m_idle,        [](DiskSample& d, double v) { d.busy_percent = detail::clamp_percent(100.0 - v); });
        gather(m_win.get().m_latency,     [](DiskSample& d, double v) { d.latency_ms = v * 1000.0; });
    }

    const bool refresh_temps = (m_samples++ % 5) == 0;

    for (auto& kv : disks) {
        DiskInfo& info = m_info[kv.first];
        if (!info.probed) probe(kv.first, info);
        DiskSample& d = kv.second;
        d.model      = info.model;
        d.rotational = info.rotational;
        d.removable  = info.removable;
        d.capacity   = info.capacity;
        if (info.has_temperature && refresh_temps) {
            const auto t = temperature(kv.first);
            if (!t) info.has_temperature = false;
            d.temperature_c = t;
        }
        s.read_bytes_per_sec  += d.read_bytes_per_sec;
        s.write_bytes_per_sec += d.write_bytes_per_sec;
        s.disks.push_back(std::move(d));
    }

    if (!refresh_temps) {
        const StorageSample prev = latest();
        for (DiskSample& d : s.disks)
            for (const DiskSample& p : prev.disks) if (p.name == d.name) d.temperature_c = p.temperature_c;
    }

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

} // namespace system
} // namespace fizmo
