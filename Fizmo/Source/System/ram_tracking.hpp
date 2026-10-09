#ifndef FIZMO_SYSTEM_RAM_TRACKING_HPP
#define FIZMO_SYSTEM_RAM_TRACKING_HPP

#include "tracker.hpp"
#include "smbios.hpp"
#include "platform_linux.hpp"
#include "platform_windows.hpp"

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
    using ProcessMemoryFn = BOOL (WINAPI*)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
    using PerformanceFn   = BOOL (WINAPI*)(PPERFORMANCE_INFORMATION, DWORD);

    detail::win::Library m_kernel{ L"kernel32.dll" };
    ProcessMemoryFn      m_process_memory = nullptr;
    PerformanceFn        m_performance    = nullptr;

    void discover() {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        m_info.page_size = si.dwPageSize;
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms)) m_info.total = ms.ullTotalPhys;
        m_info.modules = detail::read_memory_modules();
        m_process_memory = m_kernel.get<ProcessMemoryFn>("K32GetProcessMemoryInfo");
        m_performance    = m_kernel.get<PerformanceFn>("K32GetPerformanceInfo");
        PROCESS_MEMORY_COUNTERS_EX pm{};
        if (m_process_memory && m_process_memory(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pm), sizeof(pm))) m_prev_faults = pm.PageFaultCount;
    }

    RAMSample collect() {
        RAMSample s;
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);

        if (GlobalMemoryStatusEx(&ms)) {
            s.total        = ms.ullTotalPhys;
            s.available    = ms.ullAvailPhys;
            s.free         = ms.ullAvailPhys;
            s.used         = s.total > s.available ? s.total - s.available : 0;
            s.commit_limit = ms.ullTotalPageFile;
            s.commit_used  = ms.ullTotalPageFile > ms.ullAvailPageFile ? ms.ullTotalPageFile - ms.ullAvailPageFile : 0;
            s.process_virtual = ms.ullTotalVirtual > ms.ullAvailVirtual ? ms.ullTotalVirtual - ms.ullAvailVirtual : 0;
            s.usage_percent = s.total ? 100.0 * static_cast<double>(s.used) / static_cast<double>(s.total) : 0.0;
            s.swap_total = s.commit_limit > s.total ? s.commit_limit - s.total : 0;
            s.swap_used  = s.commit_used > s.used ? std::min(s.commit_used - s.used, s.swap_total) : 0;
        }

        PERFORMANCE_INFORMATION pi{};
        pi.cb = sizeof(pi);
        if (m_performance && m_performance(&pi, sizeof(pi))) s.cached = static_cast<std::uint64_t>(pi.SystemCache) * pi.PageSize;

        PROCESS_MEMORY_COUNTERS_EX pm{};
        pm.cb = sizeof(pm);

        if (m_process_memory && m_process_memory(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pm), sizeof(pm))) {
            s.process_resident      = pm.WorkingSetSize;
            s.process_peak_resident = pm.PeakWorkingSetSize;
            s.process_private       = pm.PrivateUsage;
            const detail::Clock::time_point now = detail::Clock::now();
            s.process_page_faults_per_sec = detail::per_second(pm.PageFaultCount, m_prev_faults, detail::seconds_between(m_prev_time, now));
            m_prev_faults = pm.PageFaultCount;
            m_prev_time = now;
        }

        s.process_percent = s.total ? 100.0 * static_cast<double>(s.process_resident) / static_cast<double>(s.total) : 0.0;
        return s;
    }

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
