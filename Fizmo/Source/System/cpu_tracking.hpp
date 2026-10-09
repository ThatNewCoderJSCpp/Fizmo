#ifndef FIZMO_SYSTEM_CPU_TRACKING_HPP
#define FIZMO_SYSTEM_CPU_TRACKING_HPP

#include "tracker.hpp"
#include "platform_linux.hpp"
#include "win_library.hpp"
#include <set>

namespace fizmo {
namespace system {

struct CPUInfo {
    std::string           model;
    std::string           vendor;
    unsigned int          logical_cores  = 0;
    unsigned int          physical_cores = 0;
    std::optional<double> base_mhz;
    std::optional<double> max_mhz;
};

struct CPUCore {
    double                usage_percent = 0.0;
    std::optional<double> frequency_mhz;
};

struct CPUSample {
    double                time       = 0.0;
    double                collect_ms = 0.0;

    double                usage_percent  = 0.0;
    double                user_percent   = 0.0;
    double                kernel_percent = 0.0;
    std::optional<double> iowait_percent;
    std::vector<CPUCore>  cores;

    double                process_percent        = 0.0;
    double                process_core_percent   = 0.0;
    double                process_user_percent   = 0.0;
    double                process_kernel_percent = 0.0;
    std::uint32_t         process_threads        = 0;
    std::optional<std::uint32_t> process_handles;

    std::optional<double> frequency_mhz;
    std::optional<double> max_core_frequency_mhz;
    std::optional<double> temperature_c;
    std::vector<double>   core_temperatures_c;
    std::string           temperature_source;
    std::optional<double> context_switches_per_sec;
    std::optional<std::array<double, 3>> load_average;

    FrameSummary          frames;
};

class CPUTracking : public detail::Tracker<CPUTracking, CPUSample> {
private:
    friend class detail::Tracker<CPUTracking, CPUSample>;

    struct Times {
        std::uint64_t total  = 0;
        std::uint64_t idle   = 0;
        std::uint64_t iowait = 0;
        std::uint64_t user   = 0;
        std::uint64_t kernel = 0;
    };

    static double share(std::uint64_t part_now, std::uint64_t part_before, std::uint64_t total) noexcept;

    static void usage_from(const Times& now, const Times& before, double& busy, double& user, double& kernel, double* iowait) noexcept;

    CPUInfo                    m_info;
    Times                      m_prev_total;
    std::vector<Times>         m_prev_cores;
    std::uint64_t              m_prev_proc_user   = 0;
    std::uint64_t              m_prev_proc_kernel = 0;
    std::optional<std::uint64_t> m_prev_ctxt;
    detail::Clock::time_point  m_prev_time = detail::Clock::now();

#if defined(OS_LINUX)
    double                     m_ticks_per_second = 100.0;
    std::vector<std::string>   m_temp_dirs;
    std::string                m_temp_zone;
    std::string                m_temp_source;
    bool                       m_has_cpufreq = false;

    void discover();

    bool read_stat(Times& total, std::vector<Times>& cores, std::optional<std::uint64_t>& ctxt) const;

    void read_process(std::uint64_t& user, std::uint64_t& kernel, std::uint32_t& threads) const;

    void read_frequencies(CPUSample& s) const;

    void read_temperature(CPUSample& s) const;

    void prime();

    CPUSample collect();

#elif defined(OS_WINDOWS)
    struct WinState;
    detail::Opaque<WinState> m_win;

    struct PowerInfo {
        unsigned long Number;
        unsigned long MaxMhz;
        unsigned long CurrentMhz;
        unsigned long MhzLimit;
        unsigned long MaxIdleState;
        unsigned long CurrentIdleState;
    };

    void discover();

    std::vector<PowerInfo> read_power() const;

    bool read_cores(std::vector<Times>& cores) const;

    static Times read_total();

    static void read_process(std::uint64_t& user, std::uint64_t& kernel);

    static std::uint32_t count_threads();

    void prime();

    CPUSample collect();

#else
    void discover() {}
    void prime() {}
    CPUSample collect() { CPUSample s; s.frames = take_frames(); return s; }
#endif

public:
    explicit CPUTracking(std::chrono::milliseconds interval = std::chrono::milliseconds(500), std::size_t history = 240)
        : Tracker(interval, history) {
        discover();
        prime();
    }

    ~CPUTracking() { shutdown(); }

    const CPUInfo& info() const noexcept { return m_info; }

    std::string report() const;
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_CPU_TRACKING_HPP
