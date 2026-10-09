#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "ram_tracking.hpp"

namespace fizmo {
namespace system {

#if defined(OS_LINUX)
auto RAMTracking::read_faults() const -> std::uint64_t {
        std::string text;
        if (!detail::lnx::read_text("/proc/self/stat", text)) return 0;
        const std::size_t close = text.rfind(')');
        if (close == std::string::npos) return 0;
        const std::vector<std::string> f = detail::split_ws(text.substr(close + 1));
        if (f.size() < 10) return 0;
        return detail::parse_u64(f[7]).value_or(0) + detail::parse_u64(f[9]).value_or(0);
    }
#endif

#if defined(OS_LINUX)
auto RAMTracking::discover() -> void {
        const long page = ::sysconf(_SC_PAGESIZE);
        m_info.page_size = page > 0 ? static_cast<std::uint64_t>(page) : 4096u;
        const auto mem = detail::lnx::key_values("/proc/meminfo");
        m_info.total = detail::lnx::value_of(mem, "MemTotal").value_or(0);
        m_info.modules = detail::read_memory_modules();
        for (const auto& h : detail::lnx::hwmon_by_name()) if (h.first == "spd5118" || h.first == "jc42" || h.first == "ee1004") m_dimm_sensors.push_back(h.second);
        m_prev_faults = read_faults();
    }
#endif

#if defined(OS_LINUX)
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

auto RAMTracking::report() const -> std::string {
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
