#ifndef FIZMO_SYSTEM_STORAGE_TRACKING_HPP
#define FIZMO_SYSTEM_STORAGE_TRACKING_HPP

#include "tracker.hpp"
#include "platform_linux.hpp"
#include "win_library.hpp"
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

    struct WinState;
    detail::Opaque<WinState> m_win;

    std::map<int, DiskInfo>   m_info;
    unsigned int              m_samples = 0;

    static void* open_disk(int index);

    static void probe(int index, DiskInfo& d);

    static std::optional<double> temperature(int index);

    static void read_process(std::uint64_t& read, std::uint64_t& write);

    static void read_volumes(StorageSample& out);

    void discover();

    static int disk_index(const std::string& instance);

    StorageSample collect();

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
