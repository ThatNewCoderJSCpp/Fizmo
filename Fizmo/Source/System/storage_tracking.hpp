#ifndef FIZMO_SYSTEM_STORAGE_TRACKING_HPP
#define FIZMO_SYSTEM_STORAGE_TRACKING_HPP

#include "tracker.hpp"
#include "platform_linux.hpp"
#include "platform_windows.hpp"
#include <set>

namespace fizmo {
namespace system {

struct VolumeSample {
    std::string   path;
    std::string   device;
    std::string   label;
    std::string   filesystem;
    std::uint64_t total         = 0;
    std::uint64_t free          = 0;
    std::uint64_t available     = 0;
    double        usage_percent = 0.0;
    bool          removable     = false;
    bool          read_only     = false;
};

struct DiskSample {
    std::string                  name;
    std::string                  model;
    std::optional<bool>          rotational;
    bool                         removable = false;
    std::optional<std::uint64_t> capacity;
    double                       read_bytes_per_sec  = 0.0;
    double                       write_bytes_per_sec = 0.0;
    double                       reads_per_sec       = 0.0;
    double                       writes_per_sec      = 0.0;
    std::optional<double>        busy_percent;
    std::optional<double>        latency_ms;
    std::optional<double>        temperature_c;
    std::optional<std::uint64_t> total_read_bytes;
    std::optional<std::uint64_t> total_written_bytes;
};

struct StorageSample {
    double time       = 0.0;
    double collect_ms = 0.0;

    std::vector<VolumeSample> volumes;
    std::vector<DiskSample>   disks;
    double                    read_bytes_per_sec  = 0.0;
    double                    write_bytes_per_sec = 0.0;

    double        process_read_bytes_per_sec  = 0.0;
    double        process_write_bytes_per_sec = 0.0;
    std::uint64_t process_read_bytes          = 0;
    std::uint64_t process_written_bytes       = 0;
};

class StorageTracking : public detail::Tracker<StorageTracking, StorageSample> {
private:
    friend class detail::Tracker<StorageTracking, StorageSample>;

    detail::Clock::time_point m_prev_time = detail::Clock::now();
    std::uint64_t             m_prev_proc_read  = 0;
    std::uint64_t             m_prev_proc_write = 0;

    static void finish_volume(VolumeSample& v);

#if defined(OS_LINUX)
    struct DiskState {
        std::string   name;
        std::string   model;
        std::optional<bool> rotational;
        bool          removable = false;
        std::optional<std::uint64_t> capacity;
        std::string   temp_input;
        std::uint64_t reads = 0, read_sectors = 0, read_ticks = 0;
        std::uint64_t writes = 0, write_sectors = 0, write_ticks = 0;
        std::uint64_t io_ticks = 0;
        bool          primed = false;
    };

    std::vector<DiskState> m_disks;

    static bool skip_block(const std::string& n);

    void discover();

    void read_disks(StorageSample* out, double wall);

    static void read_process(std::uint64_t& read, std::uint64_t& write);

    static std::string unescape_mount(const std::string& s);

    static void read_volumes(StorageSample& out);

    StorageSample collect();

#elif defined(OS_WINDOWS)
    struct DiskInfo {
        std::string         model;
        std::optional<bool> rotational;
        bool                removable = false;
        std::optional<std::uint64_t> capacity;
        bool                probed = false;
        bool                has_temperature = true;
    };

    std::unique_ptr<detail::win::Pdh> m_pdh;
    detail::win::Pdh::Counter m_read_bytes  = nullptr;
    detail::win::Pdh::Counter m_write_bytes = nullptr;
    detail::win::Pdh::Counter m_reads       = nullptr;
    detail::win::Pdh::Counter m_writes      = nullptr;
    detail::win::Pdh::Counter m_idle        = nullptr;
    detail::win::Pdh::Counter m_latency     = nullptr;
    std::map<int, DiskInfo>   m_info;
    std::vector<detail::win::Pdh::Item> m_items;
    unsigned int              m_samples = 0;

    static HANDLE open_disk(int index) {
        const std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(index);
        return CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    }

    static void probe(int index, DiskInfo& d) {
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

    static std::optional<double> temperature(int index) {
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

    static void read_process(std::uint64_t& read, std::uint64_t& write) {
        IO_COUNTERS io{};
        if (!GetProcessIoCounters(GetCurrentProcess(), &io)) return;
        read  = io.ReadTransferCount;
        write = io.WriteTransferCount;
    }

    static void read_volumes(StorageSample& out) {
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

    void discover() {
        m_pdh = std::make_unique<detail::win::Pdh>();
        if (m_pdh->valid()) {
            m_read_bytes  = m_pdh->add(L"\\PhysicalDisk(*)\\Disk Read Bytes/sec");
            m_write_bytes = m_pdh->add(L"\\PhysicalDisk(*)\\Disk Write Bytes/sec");
            m_reads       = m_pdh->add(L"\\PhysicalDisk(*)\\Disk Reads/sec");
            m_writes      = m_pdh->add(L"\\PhysicalDisk(*)\\Disk Writes/sec");
            m_idle        = m_pdh->add(L"\\PhysicalDisk(*)\\% Idle Time");
            m_latency     = m_pdh->add(L"\\PhysicalDisk(*)\\Avg. Disk sec/Transfer");
            m_pdh->collect();
        }
        read_process(m_prev_proc_read, m_prev_proc_write);
    }

    static int disk_index(const std::string& instance) {
        if (instance.empty() || instance[0] < '0' || instance[0] > '9') return -1;
        return std::atoi(instance.c_str());
    }

    StorageSample collect() {
        StorageSample s;
        const detail::Clock::time_point now = detail::Clock::now();
        const double wall = detail::seconds_between(m_prev_time, now);
        std::map<int, DiskSample> disks;

        if (m_pdh && m_pdh->valid() && m_pdh->collect()) {
            auto gather = [&](detail::win::Pdh::Counter c, const std::function<void(DiskSample&, double)>& apply) {
                if (!m_pdh->items(c, m_items)) return;
                for (const auto& it : m_items) {
                    const int idx = disk_index(it.name);
                    if (idx < 0) continue;
                    DiskSample& d = disks[idx];
                    if (d.name.empty()) d.name = it.name;
                    apply(d, it.value);
                }
            };
            gather(m_read_bytes,  [](DiskSample& d, double v) { d.read_bytes_per_sec = v; });
            gather(m_write_bytes, [](DiskSample& d, double v) { d.write_bytes_per_sec = v; });
            gather(m_reads,       [](DiskSample& d, double v) { d.reads_per_sec = v; });
            gather(m_writes,      [](DiskSample& d, double v) { d.writes_per_sec = v; });
            gather(m_idle,        [](DiskSample& d, double v) { d.busy_percent = detail::clamp_percent(100.0 - v); });
            gather(m_latency,     [](DiskSample& d, double v) { d.latency_ms = v * 1000.0; });
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

#else
    void discover() {}
    StorageSample collect() { return {}; }
#endif

public:
    explicit StorageTracking(std::chrono::milliseconds interval = std::chrono::milliseconds(1000), std::size_t history = 240)
        : Tracker(interval, history) {
        discover();
    }

    ~StorageTracking() { shutdown(); }

    std::string report() const;
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_STORAGE_TRACKING_HPP
