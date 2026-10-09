#ifndef FIZMO_SYSTEM_GPU_TRACKING_HPP
#define FIZMO_SYSTEM_GPU_TRACKING_HPP

#include "tracker.hpp"
#include "nvml.hpp"
#include "platform_linux.hpp"
#include "win_library.hpp"
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
    struct WinState;
    detail::Opaque<WinState> m_win;

    void discover();

    static bool parse_engine(const std::string& name, std::string& luid, std::string& engine, std::string& type);

    std::vector<GPUAdapterSample> sample_adapters(double);

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
