#ifndef FIZMO_SYSTEM_RAM_TRACKING_HPP
#define FIZMO_SYSTEM_RAM_TRACKING_HPP

#include "tracker.hpp"
#include "smbios.hpp"
#include "platform_linux.hpp"
#include "win_library.hpp"

namespace fizmo {
namespace system {

struct RAMInfo {
    std::uint64_t             total     = 0;
    std::uint64_t             page_size = 0;
    std::vector<MemoryModule> modules;
};

struct RAMSample {
    double        time       = 0.0;
    double        collect_ms = 0.0;

    std::uint64_t total         = 0;
    std::uint64_t available     = 0;
    std::uint64_t used          = 0;
    std::uint64_t free          = 0;
    double        usage_percent = 0.0;
    std::optional<std::uint64_t> cached;
    std::uint64_t commit_used   = 0;
    std::uint64_t commit_limit  = 0;
    std::uint64_t swap_total    = 0;
    std::uint64_t swap_used     = 0;

    std::uint64_t process_resident      = 0;
    std::uint64_t process_peak_resident = 0;
    std::uint64_t process_private       = 0;
    std::uint64_t process_virtual       = 0;
    std::optional<std::uint64_t> process_swap;
    double        process_percent       = 0.0;
    double        process_page_faults_per_sec = 0.0;

    std::optional<double> temperature_c;
    std::vector<double>   module_temperatures_c;
};

class RAMTracking : public detail::Tracker<RAMTracking, RAMSample> {
private:
    friend class detail::Tracker<RAMTracking, RAMSample>;

    RAMInfo                   m_info;
    std::uint64_t             m_prev_faults = 0;
    detail::Clock::time_point m_prev_time   = detail::Clock::now();

#if defined(OS_LINUX)
    std::vector<std::string> m_dimm_sensors;

    std::uint64_t read_faults() const;

    void discover();

    RAMSample collect();

#elif defined(OS_WINDOWS)
    struct WinState;
    detail::Opaque<WinState> m_win;

    void discover();

    RAMSample collect();

#else
    void discover() {}
    RAMSample collect() { return {}; }
#endif

public:
    explicit RAMTracking(std::chrono::milliseconds interval = std::chrono::milliseconds(1000), std::size_t history = 240)
        : Tracker(interval, history) {
        discover();
    }

    ~RAMTracking() { shutdown(); }

    const RAMInfo& info() const noexcept { return m_info; }

    std::string report() const;
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_RAM_TRACKING_HPP
