#ifndef FIZMO_SYSTEM_NVML_HPP
#define FIZMO_SYSTEM_NVML_HPP

#include "common.hpp"
#include "platform_linux.hpp"
#include "platform_windows.hpp"

namespace fizmo {
namespace system {
namespace detail {

class Nvml {
public:
    struct Device {
        void*         handle    = nullptr;
        std::string   name;
        std::string   pci_bus;
        std::uint32_t vendor_id = 0;
        std::uint32_t device_id = 0;
    };

    struct Reading {
        std::optional<double>        utilization_percent;
        std::optional<double>        memory_controller_percent;
        std::optional<double>        temperature_c;
        std::optional<double>        power_watts;
        std::optional<double>        power_limit_watts;
        std::optional<double>        clock_mhz;
        std::optional<double>        max_clock_mhz;
        std::optional<double>        memory_clock_mhz;
        std::optional<double>        fan_percent;
        std::optional<std::uint64_t> memory_used;
        std::optional<std::uint64_t> memory_total;
    };

private:
    struct PciInfo {
        char         bus_id_legacy[16];
        unsigned int domain;
        unsigned int bus;
        unsigned int device;
        unsigned int pci_device_id;
        unsigned int pci_subsystem_id;
        char         bus_id[32];
    };

    struct Utilization {
        unsigned int gpu;
        unsigned int memory;
    };

    struct Memory {
        unsigned long long total;
        unsigned long long free;
        unsigned long long used;
    };

    using InitFn        = int (*)();
    using ShutdownFn    = int (*)();
    using CountFn       = int (*)(unsigned int*);
    using HandleFn      = int (*)(unsigned int, void**);
    using NameFn        = int (*)(void*, char*, unsigned int);
    using PciFn         = int (*)(void*, PciInfo*);
    using UtilFn        = int (*)(void*, Utilization*);
    using MemoryFn      = int (*)(void*, Memory*);
    using TempFn        = int (*)(void*, int, unsigned int*);
    using UintFn        = int (*)(void*, unsigned int*);
    using ClockFn       = int (*)(void*, int, unsigned int*);

#if defined(OS_LINUX)
    lnx::SharedLibrary m_lib;
#elif defined(OS_WINDOWS)
    win::Library m_lib;
#endif

    bool       m_ready    = false;
    ShutdownFn m_shutdown = nullptr;
    UtilFn     m_util     = nullptr;
    MemoryFn   m_memory   = nullptr;
    TempFn     m_temp     = nullptr;
    UintFn     m_power    = nullptr;
    UintFn     m_limit    = nullptr;
    ClockFn    m_clock    = nullptr;
    ClockFn    m_max      = nullptr;
    UintFn     m_fan      = nullptr;
    std::vector<Device> m_devices;

#if defined(OS_WINDOWS)
    static win::Library open_library() {
        win::Library lib(L"nvml.dll");
        if (lib.valid()) return lib;
        wchar_t path[MAX_PATH];
        const DWORD n = ExpandEnvironmentStringsW(L"%ProgramW6432%\\NVIDIA Corporation\\NVSMI\\nvml.dll", path, MAX_PATH);
        if (n == 0 || n > MAX_PATH) return lib;
        return win::Library(path, false);
    }
#endif

public:
#if defined(OS_LINUX)
    Nvml() : m_lib("libnvidia-ml.so.1") { load(); }
#elif defined(OS_WINDOWS)
    Nvml() : m_lib(open_library()) { load(); }
#else
    Nvml() {}
#endif

    ~Nvml() { if (m_ready && m_shutdown) m_shutdown(); }
    Nvml(const Nvml&) = delete;
    Nvml& operator=(const Nvml&) = delete;

    bool valid() const noexcept { return m_ready; }
    const std::vector<Device>& devices() const noexcept { return m_devices; }

    Reading read(const Device& d) const {
        Reading r;
        if (!m_ready || !d.handle) return r;
        Utilization u{};
        if (m_util && m_util(d.handle, &u) == 0) { r.utilization_percent = u.gpu; r.memory_controller_percent = u.memory; }
        Memory m{};
        if (m_memory && m_memory(d.handle, &m) == 0) { r.memory_used = m.used; r.memory_total = m.total; }
        unsigned int v = 0;
        if (m_temp && m_temp(d.handle, 0, &v) == 0) r.temperature_c = v;
        if (m_power && m_power(d.handle, &v) == 0) r.power_watts = v / 1000.0;
        if (m_limit && m_limit(d.handle, &v) == 0) r.power_limit_watts = v / 1000.0;
        if (m_clock && m_clock(d.handle, 0, &v) == 0) r.clock_mhz = v;
        if (m_max && m_max(d.handle, 0, &v) == 0) r.max_clock_mhz = v;
        if (m_clock && m_clock(d.handle, 2, &v) == 0) r.memory_clock_mhz = v;
        if (m_fan && m_fan(d.handle, &v) == 0) r.fan_percent = v;
        return r;
    }

private:
    void load() {
#if defined(OS_LINUX) || defined(OS_WINDOWS)
        if (!m_lib.valid()) return;
        const InitFn init = m_lib.get<InitFn>("nvmlInit_v2");
        m_shutdown = m_lib.get<ShutdownFn>("nvmlShutdown");
        const CountFn count = m_lib.get<CountFn>("nvmlDeviceGetCount_v2");
        const HandleFn handle = m_lib.get<HandleFn>("nvmlDeviceGetHandleByIndex_v2");
        const NameFn name = m_lib.get<NameFn>("nvmlDeviceGetName");
        PciFn pci = m_lib.get<PciFn>("nvmlDeviceGetPciInfo_v3");
        if (!pci) pci = m_lib.get<PciFn>("nvmlDeviceGetPciInfo_v2");
        m_util   = m_lib.get<UtilFn>("nvmlDeviceGetUtilizationRates");
        m_memory = m_lib.get<MemoryFn>("nvmlDeviceGetMemoryInfo");
        m_temp   = m_lib.get<TempFn>("nvmlDeviceGetTemperature");
        m_power  = m_lib.get<UintFn>("nvmlDeviceGetPowerUsage");
        m_limit  = m_lib.get<UintFn>("nvmlDeviceGetEnforcedPowerLimit");
        m_clock  = m_lib.get<ClockFn>("nvmlDeviceGetClockInfo");
        m_max    = m_lib.get<ClockFn>("nvmlDeviceGetMaxClockInfo");
        m_fan    = m_lib.get<UintFn>("nvmlDeviceGetFanSpeed");
        if (!init || !count || !handle || init() != 0) return;
        m_ready = true;
        unsigned int n = 0;
        if (count(&n) != 0) return;

        for (unsigned int i = 0; i < n && i < 64; ++i) {
            Device d;
            if (handle(i, &d.handle) != 0 || !d.handle) continue;
            char buf[128] = {};
            if (name && name(d.handle, buf, sizeof(buf)) == 0) d.name = trim(buf);
            PciInfo p{};

            if (pci && pci(d.handle, &p) == 0) {
                char bus[32];
                std::snprintf(bus, sizeof(bus), "%04x:%02x:%02x.0", p.domain & 0xFFFFu, p.bus, p.device);
                d.pci_bus   = bus;
                d.vendor_id = p.pci_device_id & 0xFFFFu;
                d.device_id = p.pci_device_id >> 16;
            }

            m_devices.push_back(d);
        }
#endif
    }
};

} // namespace detail
} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_NVML_HPP
