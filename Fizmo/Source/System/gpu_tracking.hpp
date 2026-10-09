#ifndef FIZMO_SYSTEM_GPU_TRACKING_HPP
#define FIZMO_SYSTEM_GPU_TRACKING_HPP

#include "tracker.hpp"
#include "nvml.hpp"
#include "platform_linux.hpp"
#include "platform_windows.hpp"
#include <set>

namespace fizmo {
namespace system {

struct GPUEngine {
    std::string           name;
    double                utilization_percent = 0.0;
    std::optional<double> process_percent;
};

struct GPUAdapterInfo {
    std::string                  name;
    std::string                  vendor;
    std::string                  driver;
    std::string                  pci_bus;
    std::uint32_t                vendor_id  = 0;
    std::uint32_t                device_id  = 0;
    bool                         integrated = false;
    bool                         luid_valid = false;
    std::uint64_t                luid       = 0;
    std::optional<std::uint64_t> memory_total;
    std::optional<std::uint64_t> shared_total;
};

struct GPUAdapterSample {
    std::string   name;
    std::string   vendor;
    std::uint32_t vendor_id  = 0;
    std::uint32_t device_id  = 0;
    bool          integrated = false;
    bool          active     = false;
    std::string   source;

    std::optional<double>        utilization_percent;
    std::optional<double>        process_percent;
    std::optional<double>        memory_controller_percent;
    std::vector<GPUEngine>       engines;

    std::optional<std::uint64_t> memory_used;
    std::optional<std::uint64_t> memory_total;
    std::optional<std::uint64_t> shared_used;
    std::optional<std::uint64_t> shared_total;
    std::optional<std::uint64_t> process_memory;

    std::optional<double> temperature_c;
    std::optional<double> hotspot_c;
    std::optional<double> memory_temperature_c;
    std::optional<double> power_watts;
    std::optional<double> power_limit_watts;
    std::optional<double> power_percent;
    std::optional<double> clock_mhz;
    std::optional<double> max_clock_mhz;
    std::optional<double> memory_clock_mhz;
    std::optional<double> fan_rpm;
    std::optional<double> fan_percent;
};

struct GPUSample {
    double time       = 0.0;
    double collect_ms = 0.0;

    std::vector<GPUAdapterSample> adapters;
    int                           active_adapter = -1;

    std::string          backend;
    std::string          device_name;
    FrameSummary         frames;
    windows::GpuTimings  timings;
    windows::GpuMemory   renderer_memory;
    std::uint64_t        mesh_bytes = 0;

    const GPUAdapterSample* active() const noexcept {
        if (active_adapter >= 0 && static_cast<std::size_t>(active_adapter) < adapters.size()) return &adapters[static_cast<std::size_t>(active_adapter)];
        return nullptr;
    }

    const GPUAdapterSample* primary() const noexcept {
        if (const GPUAdapterSample* a = active()) return a;
        for (const GPUAdapterSample& a : adapters) if (!a.integrated) return &a;
        return adapters.empty() ? nullptr : &adapters.front();
    }
};

class GPUTracking : public detail::Tracker<GPUTracking, GPUSample> {
private:
    friend class detail::Tracker<GPUTracking, GPUSample>;

    std::vector<GPUAdapterInfo> m_info;
    detail::Nvml                m_nvml;
    std::vector<int>            m_nvml_of;
    detail::Clock::time_point   m_prev_time = detail::Clock::now();

    std::mutex          m_renderer_mutex;
    bool                m_have_caps   = false;
    gpu::Caps           m_caps;
    std::string         m_backend;
    windows::GpuTimings m_timings;
    windows::GpuMemory  m_renderer_memory;
    std::uint64_t       m_mesh_bytes  = 0;
    std::atomic<bool>   m_memory_due{ true };
    bool                m_enable_gpu_timing = true;

    void frame_hook(const windows::Renderer& r, const windows::FrameStats&) {
        std::lock_guard<std::mutex> lk(m_renderer_mutex);
        if (!r.is_bound()) return;
        if (!m_have_caps) {
            m_caps = r.device_caps();
            m_backend = r.backend_name();
            m_have_caps = true;
        }
        m_timings = r.gpu_timings();
        if (m_memory_due.exchange(false)) {
            m_renderer_memory = r.gpu_memory();
            m_mesh_bytes = r.gpu_mesh_bytes();
        }
    }

    void renderer_attached(windows::Renderer& r) {
        if (m_enable_gpu_timing && !r.gpu_timing()) r.set_gpu_timing(true);
        std::lock_guard<std::mutex> lk(m_renderer_mutex);
        m_have_caps = false;
    }

    int match_active(const gpu::Caps& c) const {
        if (c.backend == gpu::Backend::Auto || c.adapter_type == gpu::AdapterType::Cpu) return -1;

        if (c.luid_valid)
            for (std::size_t i = 0; i < m_info.size(); ++i) if (m_info[i].luid_valid && m_info[i].luid == c.luid) return static_cast<int>(i);

        if (!c.pci_bus.empty())
            for (std::size_t i = 0; i < m_info.size(); ++i) if (detail::lower(m_info[i].pci_bus) == detail::lower(c.pci_bus)) return static_cast<int>(i);

        if (c.vendor_id && c.device_id)
            for (std::size_t i = 0; i < m_info.size(); ++i) if (m_info[i].vendor_id == c.vendor_id && m_info[i].device_id == c.device_id) return static_cast<int>(i);

        if (c.vendor_id) {
            int found = -1, count = 0;
            for (std::size_t i = 0; i < m_info.size(); ++i) if (m_info[i].vendor_id == c.vendor_id) { if (found < 0) found = static_cast<int>(i); ++count; }
            if (count >= 1) return found;
        }

        return m_info.size() == 1 ? 0 : -1;
    }

    static std::string vendor_name(std::uint32_t id) {
        switch (id) {
            case 0x10DE: return "NVIDIA";
            case 0x1002: return "AMD";
            case 0x1022: return "AMD";
            case 0x8086: return "Intel";
            case 0x106B: return "Apple";
            case 0x13B5: return "ARM";
            case 0x5143: return "Qualcomm";
            case 0x1414: return "Microsoft";
            case 0x1AF4: return "Red Hat (virtio)";
            case 0x15AD: return "VMware";
            default:     return {};
        }
    }

    void apply_nvml(std::size_t index, GPUAdapterSample& a) const {
        if (index >= m_nvml_of.size() || m_nvml_of[index] < 0) return;
        const detail::Nvml::Reading r = m_nvml.read(m_nvml.devices()[static_cast<std::size_t>(m_nvml_of[index])]);
        if (!a.utilization_percent) a.utilization_percent = r.utilization_percent;
        if (!a.memory_controller_percent) a.memory_controller_percent = r.memory_controller_percent;
        if (!a.temperature_c) a.temperature_c = r.temperature_c;
        if (!a.power_watts) a.power_watts = r.power_watts;
        if (!a.power_limit_watts) a.power_limit_watts = r.power_limit_watts;
        if (!a.clock_mhz) a.clock_mhz = r.clock_mhz;
        if (!a.max_clock_mhz) a.max_clock_mhz = r.max_clock_mhz;
        if (!a.memory_clock_mhz) a.memory_clock_mhz = r.memory_clock_mhz;
        if (!a.fan_percent) a.fan_percent = r.fan_percent;
        if (!a.memory_used) a.memory_used = r.memory_used;
        if (!a.memory_total) a.memory_total = r.memory_total;
        if (a.source.empty()) a.source = "nvml";
        else if (a.source.find("nvml") == std::string::npos) a.source += "+nvml";
    }

#if defined(OS_LINUX)
    struct LinuxAdapter {
        std::string card;
        std::string dev;
        std::vector<std::string> hwmon;
        std::optional<std::uint64_t> prev_energy;
        detail::Clock::time_point prev_energy_time{};
    };

    struct EngineCounter {
        std::uint64_t busy  = 0;
        std::uint64_t total = 0;
        std::uint64_t capacity = 1;
        bool          cycles = false;
    };

    std::vector<LinuxAdapter> m_linux;
    std::map<std::string, EngineCounter> m_prev_engines;

    static std::optional<std::uint64_t> parse_size(const std::string& v) {
        const std::vector<std::string> f = detail::split_ws(v);
        if (f.empty()) return std::nullopt;
        const auto n = detail::parse_u64(f[0]);
        if (!n) return std::nullopt;
        if (f.size() < 2) return *n;
        if (f[1] == "KiB" || f[1] == "kB") return *n << 10;
        if (f[1] == "MiB") return *n << 20;
        if (f[1] == "GiB") return *n << 30;
        return *n;
    }

    static std::optional<double> max_dpm_clock(const std::string& path) {
        std::string text;
        if (!detail::lnx::read_text(path, text)) return std::nullopt;
        std::optional<double> best;
        for (const std::string& line : detail::split_lines(text)) {
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            const auto v = detail::parse_double(line.substr(colon + 1));
            if (v && (!best || *v > *best)) best = v;
        }
        return best;
    }

    void discover() {
        for (const std::string& n : detail::lnx::list("/sys/class/drm")) {
            if (!detail::starts_with(n, "card") || n.find('-') != std::string::npos) continue;
            LinuxAdapter la;
            la.card = "/sys/class/drm/" + n;
            la.dev  = la.card + "/device";
            GPUAdapterInfo info;
            info.vendor_id = static_cast<std::uint32_t>(detail::lnx::read_hex(la.dev + "/vendor").value_or(0));
            info.device_id = static_cast<std::uint32_t>(detail::lnx::read_hex(la.dev + "/device").value_or(0));
            info.driver    = detail::lnx::link_name(la.dev + "/driver");
            const std::string bus = detail::lnx::link_name(la.dev);
            if (bus.size() >= 12 && bus[4] == ':') info.pci_bus = bus;
            bool duplicate = false;
            for (const GPUAdapterInfo& e : m_info) if (!info.pci_bus.empty() && e.pci_bus == info.pci_bus) duplicate = true;
            if (duplicate) continue;

            const detail::lnx::PciName pn = detail::lnx::pci_name(info.vendor_id, info.device_id);
            info.vendor = vendor_name(info.vendor_id);
            if (info.vendor.empty()) info.vendor = pn.vendor;
            info.name = detail::lnx::read_line(la.dev + "/product_name");
            if (info.name.empty() && !pn.device.empty()) info.name = (info.vendor.empty() ? std::string() : info.vendor + " ") + pn.device;
            if (info.name.empty()) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%s GPU %04x:%04x", info.vendor.empty() ? "Unknown" : info.vendor.c_str(), info.vendor_id, info.device_id);
                info.name = buf;
            }

            info.memory_total = detail::lnx::read_u64(la.dev + "/mem_info_vram_total");
            info.shared_total = detail::lnx::read_u64(la.dev + "/mem_info_gtt_total");
            la.hwmon = detail::lnx::hwmon_dirs_under(la.dev);
            if (info.vendor_id == 0x8086) info.integrated = la.hwmon.empty();
            else if (info.vendor_id == 0x1002) info.integrated = info.memory_total && *info.memory_total < (2ull << 30);
            else if (info.vendor_id == 0x10DE) info.integrated = false;
            else info.integrated = info.vendor_id == 0x13B5 || info.vendor_id == 0x5143 || info.vendor_id == 0x106B;
            m_info.push_back(info);
            m_linux.push_back(la);
        }

        link_nvml();
        sample_process_engines(0.0, nullptr);
    }

    void read_hwmon(LinuxAdapter& la, GPUAdapterSample& a) {
        for (const std::string& h : la.hwmon) {
            for (const auto& t : detail::lnx::hwmon_temps(h)) {
                const std::string l = detail::lower(t.label);
                if (l == "junction" || l == "hotspot") { if (!a.hotspot_c) a.hotspot_c = t.celsius; }
                else if (l == "mem" || l == "vram") { if (!a.memory_temperature_c) a.memory_temperature_c = t.celsius; }
                else if (!a.temperature_c) a.temperature_c = t.celsius;
            }

            if (!a.power_watts) {
                auto p = detail::lnx::read_u64(h + "/power1_average");
                if (!p) p = detail::lnx::read_u64(h + "/power1_input");
                if (p && *p > 0) a.power_watts = *p / 1.0e6;
            }

            if (!a.power_watts) {
                if (const auto e = detail::lnx::read_u64(h + "/energy1_input")) {
                    const detail::Clock::time_point now = detail::Clock::now();
                    if (la.prev_energy) {
                        const double dt = detail::seconds_between(la.prev_energy_time, now);
                        if (dt > 0.0 && *e >= *la.prev_energy) a.power_watts = static_cast<double>(*e - *la.prev_energy) / 1.0e6 / dt;
                    }
                    la.prev_energy = e;
                    la.prev_energy_time = now;
                }
            }

            if (!a.power_limit_watts) {
                auto cap = detail::lnx::read_u64(h + "/power1_cap");
                if (!cap) cap = detail::lnx::read_u64(h + "/power1_max");
                if (cap && *cap > 0) a.power_limit_watts = *cap / 1.0e6;
            }

            if (!a.fan_rpm) if (const auto f = detail::lnx::read_u64(h + "/fan1_input")) a.fan_rpm = static_cast<double>(*f);
            if (!a.clock_mhz) if (const auto f = detail::lnx::read_u64(h + "/freq1_input")) a.clock_mhz = *f / 1.0e6;
            if (!a.memory_clock_mhz) if (const auto f = detail::lnx::read_u64(h + "/freq2_input")) a.memory_clock_mhz = *f / 1.0e6;
        }
    }

    void sample_process_engines(double wall, std::vector<GPUAdapterSample>* out) {
        std::map<std::string, EngineCounter> now;
        std::map<std::string, std::uint64_t> memory;
        std::set<std::string> clients;

        for (const std::string& fd : detail::lnx::list("/proc/self/fd")) {
            const std::string target = detail::lnx::read_link("/proc/self/fd/" + fd);
            if (!detail::starts_with(target, "/dev/dri/")) continue;
            std::string text;
            if (!detail::lnx::read_text("/proc/self/fdinfo/" + fd, text)) continue;
            std::string pdev, client;
            std::vector<std::pair<std::string, std::string>> kv;

            for (const std::string& line : detail::split_lines(text)) {
                const std::size_t colon = line.find(':');
                if (colon == std::string::npos || !detail::starts_with(line, "drm-")) continue;
                const std::string key = line.substr(0, colon);
                const std::string val = detail::trim(line.substr(colon + 1));
                if (key == "drm-pdev") pdev = val;
                else if (key == "drm-client-id") client = val;
                else kv.emplace_back(key, val);
            }

            if (pdev.empty() || !clients.insert(pdev + "#" + client).second) continue;
            std::map<std::string, std::uint64_t> capacity;
            for (const auto& p : kv) if (detail::starts_with(p.first, "drm-engine-capacity-")) capacity[p.first.substr(20)] = detail::parse_u64(p.second).value_or(1);

            for (const auto& p : kv) {
                if (detail::starts_with(p.first, "drm-engine-capacity-")) continue;
                if (detail::starts_with(p.first, "drm-engine-")) {
                    const std::string engine = p.first.substr(11);
                    EngineCounter& c = now[pdev + "|" + engine];
                    c.busy += parse_size(p.second).value_or(0);
                    const auto cap = capacity.find(engine);
                    c.capacity = cap != capacity.end() && cap->second > 0 ? cap->second : 1;
                } else if (detail::starts_with(p.first, "drm-cycles-")) {
                    const std::string engine = p.first.substr(11);
                    EngineCounter& c = now[pdev + "|" + engine];
                    c.busy += detail::parse_u64(p.second).value_or(0);
                    c.cycles = true;
                    const auto cap = capacity.find(engine);
                    c.capacity = cap != capacity.end() && cap->second > 0 ? cap->second : 1;
                } else if (detail::starts_with(p.first, "drm-total-cycles-")) {
                    now[pdev + "|" + p.first.substr(17)].total += detail::parse_u64(p.second).value_or(0);
                } else if (p.first == "drm-memory-vram" || p.first == "drm-resident-vram0" || p.first == "drm-resident-local0" || p.first == "drm-resident-vram") {
                    memory[pdev] += parse_size(p.second).value_or(0);
                }
            }
        }

        if (out && wall > 0.0) {
            for (std::size_t i = 0; i < m_info.size() && i < out->size(); ++i) {
                GPUAdapterSample& a = (*out)[i];
                const std::string prefix = detail::lower(m_info[i].pci_bus) + "|";
                std::optional<double> top;

                for (const auto& kv : now) {
                    if (detail::lower(kv.first).compare(0, prefix.size(), prefix) != 0) continue;
                    const auto prev = m_prev_engines.find(kv.first);
                    if (prev == m_prev_engines.end()) continue;
                    double pct = 0.0;
                    if (kv.second.cycles) {
                        const std::uint64_t db = kv.second.busy >= prev->second.busy ? kv.second.busy - prev->second.busy : 0;
                        const std::uint64_t dt = kv.second.total >= prev->second.total ? kv.second.total - prev->second.total : 0;
                        pct = dt > 0 ? 100.0 * static_cast<double>(db) / static_cast<double>(dt) / static_cast<double>(kv.second.capacity) : 0.0;
                    } else {
                        const std::uint64_t db = kv.second.busy >= prev->second.busy ? kv.second.busy - prev->second.busy : 0;
                        pct = 100.0 * static_cast<double>(db) / (wall * 1.0e9) / static_cast<double>(kv.second.capacity);
                    }
                    pct = detail::clamp_percent(pct);
                    const std::string engine = kv.first.substr(kv.first.find('|') + 1);
                    bool merged = false;
                    for (GPUEngine& e : a.engines) if (e.name == engine) { e.process_percent = pct; merged = true; }
                    if (!merged) { GPUEngine e; e.name = engine; e.process_percent = pct; a.engines.push_back(e); }
                    top = std::max(top.value_or(0.0), pct);
                }

                if (top) a.process_percent = top;
                const auto mem = memory.find(m_info[i].pci_bus);
                if (mem != memory.end()) a.process_memory = mem->second;
            }
        }

        m_prev_engines = std::move(now);
    }

    void link_nvml() {
        m_nvml_of.assign(m_info.size(), -1);
        if (!m_nvml.valid()) return;
        std::vector<bool> used(m_nvml.devices().size(), false);

        for (std::size_t i = 0; i < m_info.size(); ++i) {
            for (std::size_t k = 0; k < m_nvml.devices().size(); ++k) {
                if (used[k] || m_info[i].pci_bus.empty() || detail::lower(m_nvml.devices()[k].pci_bus) != detail::lower(m_info[i].pci_bus)) continue;
                m_nvml_of[i] = static_cast<int>(k);
                used[k] = true;
                if (!m_nvml.devices()[k].name.empty()) m_info[i].name = m_nvml.devices()[k].name;
            }
        }

        for (std::size_t k = 0; k < m_nvml.devices().size(); ++k) {
            if (used[k]) continue;
            const detail::Nvml::Device& d = m_nvml.devices()[k];
            GPUAdapterInfo info;
            info.name      = d.name;
            info.vendor    = "NVIDIA";
            info.vendor_id = d.vendor_id ? d.vendor_id : 0x10DE;
            info.device_id = d.device_id;
            info.pci_bus   = d.pci_bus;
            info.driver    = "nvidia";
            m_info.push_back(info);
            m_linux.push_back(LinuxAdapter{});
            m_nvml_of.push_back(static_cast<int>(k));
        }
    }

    std::vector<GPUAdapterSample> sample_adapters(double wall) {
        std::vector<GPUAdapterSample> out(m_info.size());

        for (std::size_t i = 0; i < m_info.size(); ++i) {
            const GPUAdapterInfo& info = m_info[i];
            LinuxAdapter& la = m_linux[i];
            GPUAdapterSample& a = out[i];
            a.name       = info.name;
            a.vendor     = info.vendor;
            a.vendor_id  = info.vendor_id;
            a.device_id  = info.device_id;
            a.integrated = info.integrated;
            a.memory_total = info.memory_total;
            a.shared_total = info.shared_total;
            if (la.dev.empty()) { apply_nvml(i, a); continue; }
            a.source = info.driver.empty() ? "sysfs" : info.driver + " sysfs";

            if (const auto v = detail::lnx::read_u64(la.dev + "/gpu_busy_percent")) a.utilization_percent = static_cast<double>(*v);
            if (const auto v = detail::lnx::read_u64(la.dev + "/mem_busy_percent")) a.memory_controller_percent = static_cast<double>(*v);
            a.memory_used = detail::lnx::read_u64(la.dev + "/mem_info_vram_used");
            a.shared_used = detail::lnx::read_u64(la.dev + "/mem_info_gtt_used");
            read_hwmon(la, a);

            if (!a.clock_mhz) {
                if (const auto v = detail::lnx::read_u64(la.card + "/gt_cur_freq_mhz")) a.clock_mhz = static_cast<double>(*v);
                else if (const auto x = detail::lnx::read_u64(la.dev + "/tile0/gt0/freq0/cur_freq")) a.clock_mhz = static_cast<double>(*x);
            }

            if (!a.max_clock_mhz) {
                if (const auto v = detail::lnx::read_u64(la.card + "/gt_max_freq_mhz")) a.max_clock_mhz = static_cast<double>(*v);
                else if (const auto x = detail::lnx::read_u64(la.dev + "/tile0/gt0/freq0/max_freq")) a.max_clock_mhz = static_cast<double>(*x);
                else a.max_clock_mhz = max_dpm_clock(la.dev + "/pp_dpm_sclk");
            }

            apply_nvml(i, a);
        }

        sample_process_engines(wall, &out);
        return out;
    }

#elif defined(OS_WINDOWS)
    std::unique_ptr<detail::win::Dxgi> m_dxgi;
    detail::win::Kmt                   m_kmt;
    std::unique_ptr<detail::win::Pdh>  m_pdh;
    detail::win::Pdh::Counter          m_engine    = nullptr;
    detail::win::Pdh::Counter          m_dedicated = nullptr;
    detail::win::Pdh::Counter          m_shared    = nullptr;
    std::vector<detail::win::Pdh::Item> m_items;
    std::vector<std::size_t>           m_dxgi_of;
    std::vector<LUID>                  m_luids;
    std::vector<std::string>           m_tags;
    std::string                        m_pid_tag;

    void discover() {
        m_dxgi = std::make_unique<detail::win::Dxgi>();
        const auto& list = m_dxgi->adapters();

        for (std::size_t i = 0; i < list.size(); ++i) {
            const detail::win::DxgiAdapterInfo& d = list[i];
            if (d.software) continue;
            GPUAdapterInfo info;
            info.name       = d.name;
            info.vendor_id  = d.vendor_id;
            info.device_id  = d.device_id;
            info.vendor     = vendor_name(d.vendor_id);
            info.luid_valid = true;
            info.luid       = detail::win::luid_u64(d.luid);
            info.memory_total = d.dedicated;
            info.shared_total = d.shared;
            info.integrated = d.dedicated < (512ull << 20);
            m_info.push_back(info);
            m_dxgi_of.push_back(i);
            m_luids.push_back(d.luid);
            m_tags.push_back(detail::win::luid_tag(d.luid));
        }

        m_pid_tag = "pid_" + std::to_string(GetCurrentProcessId()) + "_";
        m_pdh = std::make_unique<detail::win::Pdh>();
        if (m_pdh->valid()) {
            m_engine    = m_pdh->add(L"\\GPU Engine(*)\\Utilization Percentage");
            m_dedicated = m_pdh->add(L"\\GPU Adapter Memory(*)\\Dedicated Usage");
            m_shared    = m_pdh->add(L"\\GPU Adapter Memory(*)\\Shared Usage");
            m_pdh->collect();
        }

        m_nvml_of.assign(m_info.size(), -1);
        if (m_nvml.valid()) {
            std::vector<bool> used(m_nvml.devices().size(), false);
            for (std::size_t i = 0; i < m_info.size(); ++i) {
                if (m_info[i].vendor_id != 0x10DE) continue;
                for (std::size_t k = 0; k < m_nvml.devices().size(); ++k) {
                    if (used[k] || (m_nvml.devices()[k].device_id && m_nvml.devices()[k].device_id != m_info[i].device_id)) continue;
                    m_nvml_of[i] = static_cast<int>(k);
                    used[k] = true;
                    m_info[i].pci_bus = m_nvml.devices()[k].pci_bus;
                    break;
                }
            }
        }
    }

    static bool parse_engine(const std::string& name, std::string& luid, std::string& engine, std::string& type) {
        const std::string n = detail::lower(name);
        const std::size_t l = n.find("luid_");
        const std::size_t p = n.find("_phys_");
        const std::size_t e = n.find("_eng_");
        const std::size_t t = n.find("_engtype_");
        if (l == std::string::npos || p == std::string::npos || e == std::string::npos || t == std::string::npos || p < l || t < e) return false;
        luid   = n.substr(l, p - l);
        engine = n.substr(p, t - p);
        type   = name.substr(t + 9);
        if (type.empty()) type = "engine";
        return true;
    }

    std::vector<GPUAdapterSample> sample_adapters(double) {
        std::vector<GPUAdapterSample> out(m_info.size());
        for (std::size_t i = 0; i < m_info.size(); ++i) {
            GPUAdapterSample& a = out[i];
            a.name = m_info[i].name;
            a.vendor = m_info[i].vendor;
            a.vendor_id = m_info[i].vendor_id;
            a.device_id = m_info[i].device_id;
            a.integrated = m_info[i].integrated;
            a.memory_total = m_info[i].memory_total;
            a.shared_total = m_info[i].shared_total;
            a.source = "pdh+d3dkmt";
        }

        auto index_of = [&](const std::string& lowered) -> int {
            for (std::size_t i = 0; i < m_tags.size(); ++i) if (lowered.find(m_tags[i]) != std::string::npos) return static_cast<int>(i);
            return -1;
        };

        if (m_pdh && m_pdh->valid() && m_pdh->collect()) {
            std::vector<std::map<std::string, double>> engine_total(m_info.size()), engine_self(m_info.size());
            std::vector<std::map<std::string, std::string>> engine_type(m_info.size());

            if (m_pdh->items(m_engine, m_items)) {
                for (const auto& it : m_items) {
                    std::string luid, engine, type;
                    if (!parse_engine(it.name, luid, engine, type)) continue;
                    const int idx = index_of(luid);
                    if (idx < 0) continue;
                    const std::size_t k = static_cast<std::size_t>(idx);
                    engine_total[k][engine] += it.value;
                    engine_type[k][engine] = type;
                    if (detail::starts_with(detail::lower(it.name), m_pid_tag.c_str())) engine_self[k][engine] += it.value;
                }
            }

            for (std::size_t k = 0; k < m_info.size(); ++k) {
                GPUAdapterSample& a = out[k];
                std::map<std::string, GPUEngine> by_type;
                double top = 0.0, top_self = 0.0;
                for (const auto& e : engine_total[k]) {
                    const std::string& type = engine_type[k][e.first];
                    const double total = detail::clamp_percent(e.second);
                    const double self_v = detail::clamp_percent(engine_self[k][e.first]);
                    GPUEngine& g = by_type[type];
                    g.name = type;
                    g.utilization_percent = std::max(g.utilization_percent, total);
                    g.process_percent = std::max(g.process_percent.value_or(0.0), self_v);
                    top = std::max(top, total);
                    top_self = std::max(top_self, self_v);
                }
                if (!engine_total[k].empty()) {
                    a.utilization_percent = top;
                    a.process_percent = top_self;
                }
                for (auto& kv : by_type) a.engines.push_back(kv.second);
            }

            if (m_pdh->items(m_dedicated, m_items))
                for (const auto& it : m_items) { const int idx = index_of(detail::lower(it.name)); if (idx >= 0) out[static_cast<std::size_t>(idx)].memory_used = static_cast<std::uint64_t>(it.value); }
            if (m_pdh->items(m_shared, m_items))
                for (const auto& it : m_items) { const int idx = index_of(detail::lower(it.name)); if (idx >= 0) out[static_cast<std::size_t>(idx)].shared_used = static_cast<std::uint64_t>(it.value); }
        }

        for (std::size_t k = 0; k < m_info.size(); ++k) {
            GPUAdapterSample& a = out[k];
            const detail::win::Kmt::Perf p = m_kmt.query(m_luids[k]);
            a.temperature_c    = p.temperature_c;
            a.fan_rpm          = p.fan_rpm;
            a.power_percent    = p.power_percent;
            a.memory_clock_mhz = p.memory_clock_mhz;
            a.clock_mhz        = p.clock_mhz;
            a.max_clock_mhz    = p.max_clock_mhz;
            a.process_memory   = m_dxgi->process_local_usage(m_dxgi_of[k]);
            apply_nvml(k, a);
        }

        return out;
    }

#else
    void discover() {}
    std::vector<GPUAdapterSample> sample_adapters(double) { return {}; }
#endif

    GPUSample collect() {
        GPUSample s;
        const detail::Clock::time_point now = detail::Clock::now();
        s.adapters = sample_adapters(detail::seconds_between(m_prev_time, now));
        m_prev_time = now;
        gpu::Caps caps;
        bool have = false;
        {
            std::lock_guard<std::mutex> lk(m_renderer_mutex);
            have = m_have_caps;
            if (have) {
                caps = m_caps;
                s.backend = m_backend;
                s.device_name = m_caps.device_name;
                s.timings = m_timings;
                s.renderer_memory = m_renderer_memory;
                s.mesh_bytes = m_mesh_bytes;
            }
        }
        m_memory_due.store(true);

        if (have) {
            s.active_adapter = match_active(caps);
            if (s.active_adapter >= 0) {
                GPUAdapterSample& a = s.adapters[static_cast<std::size_t>(s.active_adapter)];
                a.active = true;
                if (caps.backend == gpu::Backend::Vulkan && !caps.device_name.empty()) a.name = caps.device_name;
            }
        }

        s.frames = take_frames();
        return s;
    }

public:
    explicit GPUTracking(std::chrono::milliseconds interval = std::chrono::milliseconds(500), std::size_t history = 240)
        : Tracker(interval, history) {
        discover();
    }

    ~GPUTracking() { shutdown(); }

    const std::vector<GPUAdapterInfo>& adapters() const noexcept { return m_info; }

    void set_enable_gpu_timing(bool enabled) noexcept { m_enable_gpu_timing = enabled; }

    std::string report() const {
        const GPUSample s = latest();
        std::string r = "GPU";
        if (!s.backend.empty()) r += "  renderer " + s.backend + (s.device_name.empty() ? "" : " on " + s.device_name);
        r += "\n";

        for (const GPUAdapterSample& a : s.adapters) {
            r += std::string("  ") + (a.active ? "* " : "  ") + a.name + (a.integrated ? " (integrated)" : "") + "\n";
            r += "      load " + format_optional(a.utilization_percent, 1, "%") + ", this app " + format_optional(a.process_percent, 1, "%");
            if (a.memory_controller_percent) r += ", memory bus " + format_optional(a.memory_controller_percent, 0, "%");
            r += "\n";
            if (a.memory_used || a.memory_total) r += "      vram " + (a.memory_used ? format_bytes(*a.memory_used) : std::string("n/a")) + " / " + (a.memory_total ? format_bytes(*a.memory_total) : std::string("n/a"));
            if (a.process_memory) r += ", this app " + format_bytes(*a.process_memory);
            if (a.memory_used || a.memory_total || a.process_memory) r += "\n";
            std::string sensors;
            if (a.temperature_c) sensors += " temp " + format_optional(a.temperature_c, 0, " C");
            if (a.hotspot_c) sensors += " hotspot " + format_optional(a.hotspot_c, 0, " C");
            if (a.power_watts) sensors += " power " + format_optional(a.power_watts, 1, " W");
            else if (a.power_percent) sensors += " power " + format_optional(a.power_percent, 1, "%");
            if (a.clock_mhz) sensors += " clock " + format_optional(a.clock_mhz, 0, " MHz");
            if (a.memory_clock_mhz) sensors += " mem " + format_optional(a.memory_clock_mhz, 0, " MHz");
            if (a.fan_rpm) sensors += " fan " + format_optional(a.fan_rpm, 0, " rpm");
            else if (a.fan_percent) sensors += " fan " + format_optional(a.fan_percent, 0, "%");
            if (!sensors.empty()) r += "     " + sensors + "\n";
        }

        if (s.frames.frames > 0) {
            r += "  frames   " + detail::fixed(s.frames.fps, 1) + " fps";
            if (s.frames.gpu_valid) r += ", gpu " + detail::fixed(s.frames.gpu_ms, 2) + " ms avg (" + detail::fixed(s.frames.gpu_ms_max, 2) + " max)";
            r += "\n";
        }

        if (s.timings.valid) {
            r += "  passes  ";
            for (std::size_t p = 0; p < windows::GpuTimings::PASSES; ++p) {
                if (s.timings.pass_ms[p] <= 0.0) continue;
                r += std::string(" ") + windows::gpu_pass_name(static_cast<windows::GpuPass>(p)) + " " + detail::fixed(s.timings.pass_ms[p], 2);
            }
            r += " ms\n";
        }

        if (s.renderer_memory.valid) {
            r += "  renderer memory " + format_bytes(s.renderer_memory.used);
            if (s.renderer_memory.budget > 0) r += " / budget " + format_bytes(s.renderer_memory.budget);
            if (s.mesh_bytes > 0) r += ", meshes " + format_bytes(s.mesh_bytes);
            r += "\n";
        }
        return r;
    }
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_GPU_TRACKING_HPP
