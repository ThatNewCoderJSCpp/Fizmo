#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "nvml.hpp"

namespace fizmo {
namespace system {
namespace detail {

auto Nvml::read(const Device& d) const -> Reading {
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

auto Nvml::load() -> void {
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

} // namespace detail
} // namespace system
} // namespace fizmo
