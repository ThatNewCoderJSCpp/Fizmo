#include "fizmo_library.hpp"
#include "ram_tracking.hpp"
#include "platform_windows.hpp"
#include "platform_macos.hpp"

namespace fizmo {
namespace system {

#if defined(OS_LINUX_KERNEL)
std::uint64_t RAMTracking::read_faults() const {
    std::string text;
    if (!detail::lnx::read_text("/proc/self/stat", text)) return 0;
    const std::size_t close = text.rfind(')');
    if (close == std::string::npos) return 0;
    const std::vector<std::string> f = detail::split_ws(text.substr(close + 1));
    if (f.size() < 10) return 0;
    return detail::parse_u64(f[7]).value_or(0) + detail::parse_u64(f[9]).value_or(0);
}
#endif

#if defined(OS_LINUX_KERNEL)
void RAMTracking::discover() {
    const long page = ::sysconf(_SC_PAGESIZE);
    m_info.page_size = page > 0 ? static_cast<std::uint64_t>(page) : 4096u;
    const auto mem = detail::lnx::key_values("/proc/meminfo");
    m_info.total = detail::lnx::value_of(mem, "MemTotal").value_or(0);
    if (m_info.total == 0) {
        struct sysinfo si{};
        if (::sysinfo(&si) == 0) m_info.total = static_cast<std::uint64_t>(si.totalram) * si.mem_unit;
    }
    m_info.modules = detail::read_memory_modules();
    for (const auto& h : detail::lnx::hwmon_by_name()) if (h.first == "spd5118" || h.first == "jc42" || h.first == "ee1004") m_dimm_sensors.push_back(h.second);
    m_prev_faults = read_faults();
}
#endif

#if defined(OS_LINUX_KERNEL)
auto RAMTracking::collect() -> RAMSample {
    RAMSample s;
    const auto mem = detail::lnx::key_values("/proc/meminfo");
    using detail::lnx::value_of;
    s.total     = value_of(mem, "MemTotal").value_or(0);
    s.free      = value_of(mem, "MemFree").value_or(0);
    const auto buffers = value_of(mem, "Buffers").value_or(0);
    const auto cached  = value_of(mem, "Cached").value_or(0);
    const auto reclaim = value_of(mem, "SReclaimable").value_or(0);
    s.available = value_of(mem, "MemAvailable").value_or(s.free + buffers + cached);
    s.used      = s.total > s.available ? s.total - s.available : 0;
    s.cached    = cached + buffers + reclaim;
    s.usage_percent = s.total ? 100.0 * static_cast<double>(s.used) / static_cast<double>(s.total) : 0.0;
    s.swap_total   = value_of(mem, "SwapTotal").value_or(0);
    const auto swap_free = value_of(mem, "SwapFree").value_or(0);
    s.swap_used    = s.swap_total > swap_free ? s.swap_total - swap_free : 0;
    s.commit_used  = value_of(mem, "Committed_AS").value_or(0);
    s.commit_limit = value_of(mem, "CommitLimit").value_or(0);
    if (s.total == 0) {
        struct sysinfo si{};
        if (::sysinfo(&si) == 0) {
            const std::uint64_t unit = si.mem_unit ? si.mem_unit : 1u;
            s.total      = static_cast<std::uint64_t>(si.totalram) * unit;
            s.free       = static_cast<std::uint64_t>(si.freeram) * unit;
            s.available  = s.free + static_cast<std::uint64_t>(si.bufferram) * unit;
            s.used       = s.total > s.available ? s.total - s.available : 0;
            s.cached     = static_cast<std::uint64_t>(si.bufferram) * unit;
            s.swap_total = static_cast<std::uint64_t>(si.totalswap) * unit;
            s.swap_used  = si.totalswap > si.freeswap ? static_cast<std::uint64_t>(si.totalswap - si.freeswap) * unit : 0;
            s.usage_percent = s.total ? 100.0 * static_cast<double>(s.used) / static_cast<double>(s.total) : 0.0;
        }
    }

    const auto st = detail::lnx::key_values("/proc/self/status");
    s.process_resident      = value_of(st, "VmRSS").value_or(0);
    s.process_peak_resident = value_of(st, "VmHWM").value_or(0);
    s.process_virtual       = value_of(st, "VmSize").value_or(0);
    s.process_private       = value_of(st, "RssAnon").value_or(s.process_resident);
    s.process_swap          = value_of(st, "VmSwap");
    s.process_percent       = s.total ? 100.0 * static_cast<double>(s.process_resident) / static_cast<double>(s.total) : 0.0;

    const std::uint64_t faults = read_faults();
    const detail::Clock::time_point now = detail::Clock::now();
    s.process_page_faults_per_sec = detail::per_second(faults, m_prev_faults, detail::seconds_between(m_prev_time, now));
    m_prev_faults = faults;
    m_prev_time = now;

    for (const std::string& dir : m_dimm_sensors)
        for (const auto& t : detail::lnx::hwmon_temps(dir)) s.module_temperatures_c.push_back(t.celsius);
    if (!s.module_temperatures_c.empty()) s.temperature_c = *std::max_element(s.module_temperatures_c.begin(), s.module_temperatures_c.end());
    return s;
}
#endif

#if defined(OS_MACOS)
std::uint64_t RAMTracking::read_faults() {
    struct proc_taskinfo ti{};
    if (::proc_pidinfo(::getpid(), PROC_PIDTASKINFO, 0, &ti, sizeof(ti)) != static_cast<int>(sizeof(ti))) return 0;
    return static_cast<std::uint64_t>(ti.pti_faults);
}

void RAMTracking::discover() {
    m_info.page_size = detail::mac::page_size();
    m_info.total = detail::mac::sysctl_u64("hw.memsize").value_or(0);
    m_info.modules = detail::read_memory_modules();
    m_prev_faults = read_faults();
}

auto RAMTracking::collect() -> RAMSample {
    RAMSample s;
    s.total = m_info.total ? m_info.total : detail::mac::sysctl_u64("hw.memsize").value_or(0);
    const std::uint64_t page = m_info.page_size ? m_info.page_size : detail::mac::page_size();
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    if (::host_statistics64(::mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS) {
        const std::uint64_t internal = vm.internal_page_count > vm.purgeable_count ? vm.internal_page_count - vm.purgeable_count : 0;
        const std::uint64_t used = (internal + vm.wire_count + vm.compressor_page_count) * page;
        s.used = std::min(used, s.total);
        s.available = s.total > s.used ? s.total - s.used : 0;
        s.free = static_cast<std::uint64_t>(vm.free_count) * page;
        s.cached = (static_cast<std::uint64_t>(vm.external_page_count) + vm.purgeable_count) * page;
        s.usage_percent = s.total ? 100.0 * static_cast<double>(s.used) / static_cast<double>(s.total) : 0.0;
    }

    struct xsw_usage swap{};
    std::size_t swap_size = sizeof(swap);
    if (::sysctlbyname("vm.swapusage", &swap, &swap_size, nullptr, 0) == 0) {
        s.swap_total = swap.xsu_total;
        s.swap_used  = swap.xsu_used;
    }
    s.commit_used  = s.used + s.swap_used;
    s.commit_limit = s.total + s.swap_total;

    struct proc_taskinfo ti{};
    if (::proc_pidinfo(::getpid(), PROC_PIDTASKINFO, 0, &ti, sizeof(ti)) == static_cast<int>(sizeof(ti))) {
        s.process_resident = ti.pti_resident_size;
        s.process_virtual  = ti.pti_virtual_size;
    }

    task_vm_info_data_t vmi{};
    mach_msg_type_number_t vmi_count = TASK_VM_INFO_COUNT;
    if (::task_info(::mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&vmi), &vmi_count) == KERN_SUCCESS) s.process_private = vmi.phys_footprint;
    else s.process_private = s.process_resident;

    struct rusage ru{};
    if (::getrusage(RUSAGE_SELF, &ru) == 0) s.process_peak_resident = static_cast<std::uint64_t>(ru.ru_maxrss);
    s.process_percent = s.total ? 100.0 * static_cast<double>(s.process_resident) / static_cast<double>(s.total) : 0.0;

    const std::uint64_t faults = read_faults();
    const detail::Clock::time_point now = detail::Clock::now();
    s.process_page_faults_per_sec = detail::per_second(faults, m_prev_faults, detail::seconds_between(m_prev_time, now));
    m_prev_faults = faults;
    m_prev_time = now;
    return s;
}
#endif

std::string RAMTracking::report() const {
    const RAMSample s = latest();
    std::string r = "RAM  " + format_bytes(m_info.total);
    if (!m_info.modules.empty()) {
        const MemoryModule& m = m_info.modules.front();
        r += "  (" + std::to_string(m_info.modules.size()) + " x " + format_bytes(m.size_bytes);
        if (!m.type.empty()) r += " " + m.type;
        if (m.configured_speed_mts || m.speed_mts) r += "-" + std::to_string(m.configured_speed_mts ? m.configured_speed_mts : m.speed_mts);
        r += ")";
    }
    r += "\n";
    r += "  used     " + format_bytes(s.used) + " / " + format_bytes(s.total) + "  (" + detail::fixed(s.usage_percent, 1) + "%), available " + format_bytes(s.available) + "\n";
    if (s.cached) r += "  cached   " + format_bytes(*s.cached) + "\n";
    r += "  commit   " + format_bytes(s.commit_used) + " / " + format_bytes(s.commit_limit) + ", swap " + format_bytes(s.swap_used) + " / " + format_bytes(s.swap_total) + "\n";
    r += "  process  " + format_bytes(s.process_resident) + " resident (peak " + format_bytes(s.process_peak_resident) + "), " + format_bytes(s.process_private) + " private, " + detail::fixed(s.process_page_faults_per_sec, 0) + " faults/s\n";
    if (s.temperature_c) r += "  temp     " + format_optional(s.temperature_c, 1, " C") + "\n";
    return r;
}

} // namespace system
} // namespace fizmo

namespace fizmo {
namespace system {

#if defined(OS_WINDOWS)
struct RAMTracking::WinState {
    using ProcessMemoryFn = BOOL (WINAPI*)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
    using PerformanceFn   = BOOL (WINAPI*)(PPERFORMANCE_INFORMATION, DWORD);
    detail::win::Library m_kernel{ L"kernel32.dll" };
    ProcessMemoryFn      m_process_memory = nullptr;
    PerformanceFn        m_performance    = nullptr;
};
#endif
#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
void RAMTracking::discover() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    m_info.page_size = si.dwPageSize;
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) m_info.total = ms.ullTotalPhys;
    m_info.modules = detail::read_memory_modules();
    m_win.get().m_process_memory = m_win.get().m_kernel.get<WinState::ProcessMemoryFn>("K32GetProcessMemoryInfo");
    m_win.get().m_performance    = m_win.get().m_kernel.get<WinState::PerformanceFn>("K32GetPerformanceInfo");
    PROCESS_MEMORY_COUNTERS_EX pm{};
    if (m_win.get().m_process_memory && m_win.get().m_process_memory(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pm), sizeof(pm))) m_prev_faults = pm.PageFaultCount;
}
#endif

#if !(defined(OS_LINUX)) && (defined(OS_WINDOWS))
auto RAMTracking::collect() -> RAMSample {
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
    if (m_win.get().m_performance && m_win.get().m_performance(&pi, sizeof(pi))) s.cached = static_cast<std::uint64_t>(pi.SystemCache) * pi.PageSize;

    PROCESS_MEMORY_COUNTERS_EX pm{};
    pm.cb = sizeof(pm);

    if (m_win.get().m_process_memory && m_win.get().m_process_memory(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pm), sizeof(pm))) {
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
#endif

} // namespace system
} // namespace fizmo
