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

    const GPUAdapterSample* active() const noexcept;

    const GPUAdapterSample* primary() const noexcept;
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

    void frame_hook(const windows::Renderer& r, const windows::FrameStats&);

    void renderer_attached(windows::Renderer& r);

    int match_active(const gpu::Caps& c) const;

    static std::string vendor_name(std::uint32_t id);

    void apply_nvml(std::size_t index, GPUAdapterSample& a) const;

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

    static std::optional<std::uint64_t> parse_size(const std::string& v);

    static std::optional<double> max_dpm_clock(const std::string& path);

    void discover();

    void read_hwmon(LinuxAdapter& la, GPUAdapterSample& a);

    void sample_process_engines(double wall, std::vector<GPUAdapterSample>* out);

    void link_nvml();

    std::vector<GPUAdapterSample> sample_adapters(double wall);

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

    GPUSample collect();

public:
    explicit GPUTracking(std::chrono::milliseconds interval = std::chrono::milliseconds(500), std::size_t history = 240)
        : Tracker(interval, history) {
        discover();
    }

    ~GPUTracking() { shutdown(); }

    const std::vector<GPUAdapterInfo>& adapters() const noexcept { return m_info; }

    void set_enable_gpu_timing(bool enabled) noexcept { m_enable_gpu_timing = enabled; }

    std::string report() const;
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_GPU_TRACKING_HPP
