#ifndef FIZMO_SYSTEM_CPU_TRACKING_HPP
#define FIZMO_SYSTEM_CPU_TRACKING_HPP

#include "tracker.hpp"
#include "platform_linux.hpp"
#include "platform_windows.hpp"
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
    struct ProcessorPerf {
        LARGE_INTEGER IdleTime;
        LARGE_INTEGER KernelTime;
        LARGE_INTEGER UserTime;
        LARGE_INTEGER DpcTime;
        LARGE_INTEGER InterruptTime;
        ULONG         InterruptCount;
    };

    struct PowerInfo {
        ULONG Number;
        ULONG MaxMhz;
        ULONG CurrentMhz;
        ULONG MhzLimit;
        ULONG MaxIdleState;
        ULONG CurrentIdleState;
    };

    using QuerySystemFn = LONG (WINAPI*)(ULONG, PVOID, ULONG, PULONG);
    using PowerFn       = LONG (WINAPI*)(int, PVOID, ULONG, PVOID, ULONG);

    detail::win::Library        m_ntdll{ L"ntdll.dll" };
    detail::win::Library        m_powrprof{ L"powrprof.dll" };
    QuerySystemFn               m_query_system = nullptr;
    PowerFn                     m_power        = nullptr;
    std::unique_ptr<detail::win::Pdh> m_pdh;
    detail::win::Pdh::Counter   m_perf_total = nullptr;
    detail::win::Pdh::Counter   m_perf_cores = nullptr;
    detail::win::Pdh::Counter   m_thermal    = nullptr;
    detail::win::Pdh::Counter   m_ctxt       = nullptr;
    std::vector<detail::win::Pdh::Item> m_items;

    void discover() {
        m_info.model  = detail::win::registry_string(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
        m_info.vendor = detail::win::registry_string(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"VendorIdentifier");
        if (m_info.vendor == "GenuineIntel") m_info.vendor = "Intel";
        else if (m_info.vendor == "AuthenticAMD") m_info.vendor = "AMD";
        const DWORD logical = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        m_info.logical_cores = logical > 0 ? logical : 1;

        DWORD len = 0;
        GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
        if (len > 0) {
            std::vector<unsigned char> buf(len);
            if (GetLogicalProcessorInformationEx(RelationProcessorCore, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()), &len)) {
                unsigned int count = 0;
                for (DWORD off = 0; off < len;) {
                    const auto* e = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data() + off);
                    if (e->Size == 0) break;
                    if (e->Relationship == RelationProcessorCore) ++count;
                    off += e->Size;
                }
                m_info.physical_cores = count;
            }
        }
        if (m_info.physical_cores == 0) m_info.physical_cores = m_info.logical_cores;

        m_query_system = m_ntdll.get<QuerySystemFn>("NtQuerySystemInformation");
        m_power        = m_powrprof.get<PowerFn>("CallNtPowerInformation");

        if (const auto mhz = detail::win::registry_dword(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz")) m_info.base_mhz = *mhz;
        std::vector<PowerInfo> power = read_power();
        if (!power.empty()) {
            ULONG top = 0;
            for (const PowerInfo& p : power) top = std::max(top, p.MaxMhz);
            if (top > 0) { m_info.base_mhz = top; }
        }

        m_pdh = std::make_unique<detail::win::Pdh>();
        if (m_pdh->valid()) {
            m_perf_total = m_pdh->add(L"\\Processor Information(_Total)\\% Processor Performance");
            m_perf_cores = m_pdh->add(L"\\Processor Information(*)\\% Processor Performance");
            m_thermal    = m_pdh->add(L"\\Thermal Zone Information(*)\\Temperature");
            m_ctxt       = m_pdh->add(L"\\System\\Context Switches/sec");
            m_pdh->collect();
        }
    }

    std::vector<PowerInfo> read_power() const {
        std::vector<PowerInfo> out;
        if (!m_power) return out;
        out.resize(std::max(1u, m_info.logical_cores));
        if (m_power(11, nullptr, 0, out.data(), static_cast<ULONG>(out.size() * sizeof(PowerInfo))) != 0) out.clear();
        return out;
    }

    bool read_cores(std::vector<Times>& cores) const {
        cores.clear();
        if (!m_query_system) return false;
        std::vector<ProcessorPerf> buf(std::max(1u, m_info.logical_cores));
        ULONG got = 0;
        if (m_query_system(8, buf.data(), static_cast<ULONG>(buf.size() * sizeof(ProcessorPerf)), &got) != 0) return false;
        const std::size_t n = got / sizeof(ProcessorPerf);
        cores.resize(n);

        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t idle   = static_cast<std::uint64_t>(buf[i].IdleTime.QuadPart);
            const std::uint64_t kernel = static_cast<std::uint64_t>(buf[i].KernelTime.QuadPart);
            const std::uint64_t user   = static_cast<std::uint64_t>(buf[i].UserTime.QuadPart);
            cores[i].idle   = idle;
            cores[i].user   = user;
            cores[i].kernel = kernel > idle ? kernel - idle : 0;
            cores[i].total  = kernel + user;
        }

        return true;
    }

    static Times read_total() {
        Times t;
        FILETIME idle{}, kernel{}, user{};
        if (!GetSystemTimes(&idle, &kernel, &user)) return t;
        const std::uint64_t i = detail::win::filetime_u64(idle), k = detail::win::filetime_u64(kernel), u = detail::win::filetime_u64(user);
        t.idle   = i;
        t.user   = u;
        t.kernel = k > i ? k - i : 0;
        t.total  = k + u;
        return t;
    }

    static void read_process(std::uint64_t& user, std::uint64_t& kernel) {
        FILETIME c{}, e{}, k{}, u{};
        if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) return;
        user   = detail::win::filetime_u64(u);
        kernel = detail::win::filetime_u64(k);
    }

    static std::uint32_t count_threads() {
        const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        const DWORD pid = GetCurrentProcessId();
        THREADENTRY32 te{};
        te.dwSize = sizeof(te);
        std::uint32_t n = 0;
        if (Thread32First(snap, &te)) {
            do { if (te.th32OwnerProcessID == pid) ++n; } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
        return n;
    }

    void prime() {
        m_prev_total = read_total();
        read_cores(m_prev_cores);
        read_process(m_prev_proc_user, m_prev_proc_kernel);
        m_prev_time = detail::Clock::now();
    }

    CPUSample collect() {
        CPUSample s;
        const Times total = read_total();
        std::vector<Times> cores;
        read_cores(cores);
        std::uint64_t pu = m_prev_proc_user, pk = m_prev_proc_kernel;
        read_process(pu, pk);
        const detail::Clock::time_point now = detail::Clock::now();
        const double wall = detail::seconds_between(m_prev_time, now);

        usage_from(total, m_prev_total, s.usage_percent, s.user_percent, s.kernel_percent, nullptr);
        s.cores.resize(cores.size());

        for (std::size_t i = 0; i < cores.size(); ++i) {
            const Times before = i < m_prev_cores.size() ? m_prev_cores[i] : Times{};
            double u = 0.0, k = 0.0;
            usage_from(cores[i], before, s.cores[i].usage_percent, u, k, nullptr);
        }

        if (wall > 0.0) {
            const double logical  = std::max(1u, m_info.logical_cores);
            const double user_s   = static_cast<double>(pu >= m_prev_proc_user ? pu - m_prev_proc_user : 0) / 1.0e7;
            const double kernel_s = static_cast<double>(pk >= m_prev_proc_kernel ? pk - m_prev_proc_kernel : 0) / 1.0e7;
            s.process_core_percent   = 100.0 * (user_s + kernel_s) / wall;
            s.process_percent        = detail::clamp_percent(s.process_core_percent / logical);
            s.process_user_percent   = detail::clamp_percent(100.0 * user_s / wall / logical);
            s.process_kernel_percent = detail::clamp_percent(100.0 * kernel_s / wall / logical);
        }

        s.process_threads = count_threads();
        DWORD handles = 0;
        if (GetProcessHandleCount(GetCurrentProcess(), &handles)) s.process_handles = handles;

        if (m_pdh && m_pdh->valid() && m_pdh->collect()) {
            if (m_info.base_mhz) {
                if (const auto perf = m_pdh->value(m_perf_total)) s.frequency_mhz = *m_info.base_mhz * *perf / 100.0;

                if (m_pdh->items(m_perf_cores, m_items)) {
                    std::size_t idx = 0;
                    double top = 0.0;
                    for (const auto& it : m_items) {
                        if (it.name.find("_Total") != std::string::npos) continue;
                        const double mhz = *m_info.base_mhz * it.value / 100.0;
                        if (idx < s.cores.size()) s.cores[idx].frequency_mhz = mhz;
                        top = std::max(top, mhz);
                        ++idx;
                    }
                    if (top > 0.0) s.max_core_frequency_mhz = top;
                }
            }

            if (m_pdh->items(m_thermal, m_items)) {
                for (const auto& it : m_items) {
                    if (it.value <= 0.0) continue;
                    const double c = it.value - 273.15;
                    if (c > -50.0 && c < 150.0 && (!s.temperature_c || c > *s.temperature_c)) s.temperature_c = c;
                }
                if (s.temperature_c) s.temperature_source = "acpi thermal zone";
            }

            s.context_switches_per_sec = m_pdh->value(m_ctxt);
        }

        if (!s.frequency_mhz) {
            const std::vector<PowerInfo> power = read_power();
            if (!power.empty()) {
                double sum = 0.0, top = 0.0;
                for (std::size_t i = 0; i < power.size(); ++i) {
                    if (i < s.cores.size()) s.cores[i].frequency_mhz = power[i].CurrentMhz;
                    sum += power[i].CurrentMhz;
                    top = std::max(top, static_cast<double>(power[i].CurrentMhz));
                }
                s.frequency_mhz = sum / static_cast<double>(power.size());
                s.max_core_frequency_mhz = top;
            }
        }

        m_prev_total = total;
        m_prev_cores = std::move(cores);
        m_prev_proc_user = pu;
        m_prev_proc_kernel = pk;
        m_prev_time = now;
        s.frames = take_frames();
        return s;
    }

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
