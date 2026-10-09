#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "cpu_tracking.hpp"

namespace fizmo {
namespace system {

auto CPUTracking::share(std::uint64_t part_now, std::uint64_t part_before, std::uint64_t total) noexcept -> double {
        if (total == 0 || part_now < part_before) return 0.0;
        return detail::clamp_percent(100.0 * static_cast<double>(part_now - part_before) / static_cast<double>(total));
    }

auto CPUTracking::usage_from(const Times& now, const Times& before, double& busy, double& user, double& kernel, double* iowait) noexcept -> void {
        const std::uint64_t total = now.total >= before.total ? now.total - before.total : 0;
        const std::uint64_t idle  = (now.idle >= before.idle ? now.idle - before.idle : 0) + (now.iowait >= before.iowait ? now.iowait - before.iowait : 0);
        busy   = total > 0 ? detail::clamp_percent(100.0 * static_cast<double>(total > idle ? total - idle : 0) / static_cast<double>(total)) : 0.0;
        user   = share(now.user, before.user, total);
        kernel = share(now.kernel, before.kernel, total);
        if (iowait) *iowait = share(now.iowait, before.iowait, total);
    }

#if defined(OS_LINUX)
auto CPUTracking::discover() -> void {
        std::string text;
        std::set<std::pair<std::string, std::string>> cores;
        std::string phys;
        unsigned int processors = 0;

        if (detail::lnx::read_text("/proc/cpuinfo", text)) {
            for (const std::string& line : detail::split_lines(text)) {
                const std::size_t colon = line.find(':');
                if (colon == std::string::npos) continue;
                const std::string key = detail::trim(line.substr(0, colon));
                const std::string val = detail::trim(line.substr(colon + 1));
                if (key == "processor") ++processors;
                else if ((key == "model name" || key == "Model" || key == "Hardware" || key == "cpu model") && m_info.model.empty()) m_info.model = val;
                else if ((key == "vendor_id" || key == "CPU implementer") && m_info.vendor.empty()) m_info.vendor = val;
                else if (key == "physical id") phys = val;
                else if (key == "core id") cores.insert({ phys, val });
            }
        }

        const long online = ::sysconf(_SC_NPROCESSORS_ONLN);
        m_info.logical_cores  = processors > 0 ? processors : static_cast<unsigned int>(online > 0 ? online : 1);
        m_info.physical_cores = cores.empty() ? m_info.logical_cores : static_cast<unsigned int>(cores.size());
        if (m_info.vendor == "GenuineIntel") m_info.vendor = "Intel";
        else if (m_info.vendor == "AuthenticAMD") m_info.vendor = "AMD";

        const std::string freq = "/sys/devices/system/cpu/cpu0/cpufreq/";
        if (const auto v = detail::lnx::read_u64(freq + "cpuinfo_max_freq")) m_info.max_mhz = *v / 1000.0;
        if (const auto v = detail::lnx::read_u64(freq + "base_frequency")) m_info.base_mhz = *v / 1000.0;
        m_has_cpufreq = detail::lnx::exists(freq + "scaling_cur_freq");

        const long ticks = ::sysconf(_SC_CLK_TCK);
        if (ticks > 0) m_ticks_per_second = static_cast<double>(ticks);

        static const char* primary[]   = { "coretemp", "k10temp", "zenpower", "k8temp" };
        static const char* secondary[] = { "cpu_thermal", "cpu-thermal", "soc_thermal", "cpu" };
        const auto hw = detail::lnx::hwmon_by_name();

        for (const char* want : primary)
            for (const auto& h : hw) if (h.first == want) { m_temp_dirs.push_back(h.second); m_temp_source = want; }

        if (m_temp_dirs.empty())
            for (const char* want : secondary)
                for (const auto& h : hw) if (h.first == want && m_temp_dirs.empty()) { m_temp_dirs.push_back(h.second); m_temp_source = want; }

        if (m_temp_dirs.empty()) {
            static const char* zones[] = { "x86_pkg_temp", "cpu-thermal", "cpu_thermal", "soc_thermal", "cpu", "acpitz" };
            const auto list = detail::lnx::list("/sys/class/thermal");

            for (const char* want : zones) {
                for (const std::string& z : list) {
                    if (!detail::starts_with(z, "thermal_zone")) continue;
                    if (detail::lnx::read_line("/sys/class/thermal/" + z + "/type") != want) continue;
                    m_temp_zone = "/sys/class/thermal/" + z + "/temp";
                    m_temp_source = want;
                    break;
                }
                if (!m_temp_zone.empty()) break;
            }
        }
    }
#endif

#if defined(OS_LINUX)
auto CPUTracking::read_stat(Times& total, std::vector<Times>& cores, std::optional<std::uint64_t>& ctxt) const -> bool {
        std::string text;
        if (!detail::lnx::read_text("/proc/stat", text)) return false;
        cores.clear();

        for (const std::string& line : detail::split_lines(text)) {
            if (detail::starts_with(line, "cpu")) {
                const std::vector<std::string> f = detail::split_ws(line);
                if (f.size() < 5) continue;
                std::uint64_t v[8] = {};
                for (std::size_t i = 0; i < 8 && i + 1 < f.size(); ++i) v[i] = detail::parse_u64(f[i + 1]).value_or(0);
                Times t;
                t.user   = v[0] + v[1];
                t.kernel = v[2] + v[5] + v[6];
                t.idle   = v[3];
                t.iowait = v[4];
                t.total  = v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6] + v[7];

                if (f[0] == "cpu") total = t;
                else {
                    const auto idx = detail::parse_u64(f[0].substr(3));
                    if (!idx || *idx > 4096) continue;
                    if (cores.size() <= *idx) cores.resize(*idx + 1);
                    cores[*idx] = t;
                }
            } else if (detail::starts_with(line, "ctxt ")) {
                ctxt = detail::parse_u64(line.substr(5));
            }
        }

        return true;
    }
#endif

#if defined(OS_LINUX)
auto CPUTracking::read_process(std::uint64_t& user, std::uint64_t& kernel, std::uint32_t& threads) const -> void {
        std::string text;
        if (!detail::lnx::read_text("/proc/self/stat", text)) return;
        const std::size_t close = text.rfind(')');
        if (close == std::string::npos) return;
        const std::vector<std::string> f = detail::split_ws(text.substr(close + 1));
        if (f.size() < 18) return;
        user    = detail::parse_u64(f[11]).value_or(0);
        kernel  = detail::parse_u64(f[12]).value_or(0);
        threads = static_cast<std::uint32_t>(detail::parse_u64(f[17]).value_or(0));
    }
#endif

#if defined(OS_LINUX)
auto CPUTracking::read_frequencies(CPUSample& s) const -> void {
        std::vector<double> mhz;

        if (m_has_cpufreq) {
            for (std::size_t i = 0; i < s.cores.size(); ++i) {
                const auto v = detail::lnx::read_u64("/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/scaling_cur_freq");
                if (!v) continue;
                s.cores[i].frequency_mhz = *v / 1000.0;
                mhz.push_back(*v / 1000.0);
            }
        } else {
            std::string text;
            if (detail::lnx::read_text("/proc/cpuinfo", text)) {
                std::size_t core = 0;
                for (const std::string& line : detail::split_lines(text)) {
                    if (!detail::starts_with(line, "cpu MHz")) continue;
                    const std::size_t colon = line.find(':');
                    const auto v = colon == std::string::npos ? std::nullopt : detail::parse_double(line.substr(colon + 1));
                    if (v) {
                        if (core < s.cores.size()) s.cores[core].frequency_mhz = *v;
                        mhz.push_back(*v);
                    }
                    ++core;
                }
            }
        }

        if (mhz.empty()) return;
        double sum = 0.0, top = 0.0;
        for (double v : mhz) { sum += v; top = std::max(top, v); }
        s.frequency_mhz = sum / static_cast<double>(mhz.size());
        s.max_core_frequency_mhz = top;
    }
#endif

#if defined(OS_LINUX)
auto CPUTracking::read_temperature(CPUSample& s) const -> void {
        if (!m_temp_zone.empty()) {
            if (const auto v = detail::lnx::read_i64(m_temp_zone)) if (*v > -273000) s.temperature_c = *v / 1000.0;
            s.temperature_source = m_temp_source;
            return;
        }

        std::optional<double> package, first;

        for (const std::string& dir : m_temp_dirs) {
            for (const auto& t : detail::lnx::hwmon_temps(dir)) {
                if (!first) first = t.celsius;
                const std::string l = t.label;
                if (detail::starts_with(l, "Package") || l == "Tdie" || (l == "Tctl" && !package)) package = std::max(package.value_or(t.celsius), t.celsius);
                else if (detail::starts_with(l, "Core") || detail::starts_with(l, "Tccd")) s.core_temperatures_c.push_back(t.celsius);
            }
        }

        if (package) s.temperature_c = package;
        else if (!s.core_temperatures_c.empty()) s.temperature_c = *std::max_element(s.core_temperatures_c.begin(), s.core_temperatures_c.end());
        else s.temperature_c = first;
        if (s.temperature_c) s.temperature_source = m_temp_source;
    }
#endif

#if defined(OS_LINUX)
auto CPUTracking::prime() -> void {
        read_stat(m_prev_total, m_prev_cores, m_prev_ctxt);
        std::uint32_t threads = 0;
        read_process(m_prev_proc_user, m_prev_proc_kernel, threads);
        m_prev_time = detail::Clock::now();
    }
#endif

#if defined(OS_LINUX)
auto CPUTracking::collect() -> CPUSample {
        CPUSample s;
        Times total;
        std::vector<Times> cores;
        std::optional<std::uint64_t> ctxt;
        read_stat(total, cores, ctxt);
        std::uint64_t pu = m_prev_proc_user, pk = m_prev_proc_kernel;
        read_process(pu, pk, s.process_threads);
        const detail::Clock::time_point now = detail::Clock::now();
        const double wall = detail::seconds_between(m_prev_time, now);

        double iowait = 0.0;
        usage_from(total, m_prev_total, s.usage_percent, s.user_percent, s.kernel_percent, &iowait);
        s.iowait_percent = iowait;
        s.cores.resize(cores.size());

        for (std::size_t i = 0; i < cores.size(); ++i) {
            const Times before = i < m_prev_cores.size() ? m_prev_cores[i] : Times{};
            double u = 0.0, k = 0.0;
            usage_from(cores[i], before, s.cores[i].usage_percent, u, k, nullptr);
        }

        if (wall > 0.0) {
            const double logical = std::max(1u, m_info.logical_cores);
            const double user_s   = static_cast<double>(pu >= m_prev_proc_user ? pu - m_prev_proc_user : 0) / m_ticks_per_second;
            const double kernel_s = static_cast<double>(pk >= m_prev_proc_kernel ? pk - m_prev_proc_kernel : 0) / m_ticks_per_second;
            s.process_core_percent   = 100.0 * (user_s + kernel_s) / wall;
            s.process_percent        = detail::clamp_percent(s.process_core_percent / logical);
            s.process_user_percent   = detail::clamp_percent(100.0 * user_s / wall / logical);
            s.process_kernel_percent = detail::clamp_percent(100.0 * kernel_s / wall / logical);
            if (ctxt && m_prev_ctxt) s.context_switches_per_sec = detail::per_second(*ctxt, *m_prev_ctxt, wall);
        }

        read_frequencies(s);
        read_temperature(s);

        const std::vector<std::string> load = detail::split_ws(detail::lnx::read_line("/proc/loadavg"));
        if (load.size() >= 3) {
            std::array<double, 3> l{};
            for (int i = 0; i < 3; ++i) l[static_cast<std::size_t>(i)] = detail::parse_double(load[static_cast<std::size_t>(i)]).value_or(0.0);
            s.load_average = l;
        }

        m_prev_total = total;
        m_prev_cores = std::move(cores);
        m_prev_proc_user = pu;
        m_prev_proc_kernel = pk;
        m_prev_ctxt = ctxt;
        m_prev_time = now;
        s.frames = take_frames();
        return s;
    }
#endif

auto CPUTracking::report() const -> std::string {
        const CPUSample s = latest();
        std::string r;
        r += "CPU  " + (m_info.model.empty() ? std::string("unknown") : m_info.model) + "  (" + std::to_string(m_info.physical_cores) + "C/" + std::to_string(m_info.logical_cores) + "T)\n";
        r += "  usage    " + detail::fixed(s.usage_percent, 1) + "%  (user " + detail::fixed(s.user_percent, 1) + "%, kernel " + detail::fixed(s.kernel_percent, 1) + "%)\n";
        r += "  process  " + detail::fixed(s.process_percent, 1) + "% of machine, " + detail::fixed(s.process_core_percent, 1) + "% of one core, " + std::to_string(s.process_threads) + " threads\n";
        r += "  clock    " + format_optional(s.frequency_mhz, 0, " MHz") + "  (fastest core " + format_optional(s.max_core_frequency_mhz, 0, " MHz") + ")\n";
        r += "  temp     " + format_optional(s.temperature_c, 1, " C") + (s.temperature_source.empty() ? "" : "  [" + s.temperature_source + "]") + "\n";
        if (s.load_average) r += "  load     " + detail::fixed((*s.load_average)[0], 2) + " " + detail::fixed((*s.load_average)[1], 2) + " " + detail::fixed((*s.load_average)[2], 2) + "\n";
        if (s.frames.frames > 0) {
            r += "  frames   " + detail::fixed(s.frames.fps, 1) + " fps, " + detail::fixed(s.frames.frame_ms, 2) + " ms avg, " + detail::fixed(s.frames.frame_ms_p99, 2) + " ms p99, 1% low " + detail::fixed(s.frames.low_1_percent_fps, 1) + " fps\n";
            r += "  cpu work " + detail::fixed(s.frames.cpu_ms, 2) + " ms avg (" + detail::fixed(s.frames.cpu_ms_max, 2) + " max), present " + detail::fixed(s.frames.present_ms, 2) + " ms\n";
        }
        return r;
    }

} // namespace system
} // namespace fizmo
