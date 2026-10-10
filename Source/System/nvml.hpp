#ifndef FIZMO_SYSTEM_NVML_HPP
#define FIZMO_SYSTEM_NVML_HPP

#include "common.hpp"
#include "platform_linux.hpp"
#include "win_library.hpp"

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
    static win::Library open_library();
#endif

public:
#if defined(OS_LINUX)
    Nvml() : m_lib("libnvidia-ml.so.1") { load(); }
#elif defined(OS_WINDOWS)
    Nvml();
#else
    Nvml() {}
#endif

    ~Nvml() { if (m_ready && m_shutdown) m_shutdown(); }
    Nvml(const Nvml&) = delete;
    Nvml& operator=(const Nvml&) = delete;

    bool valid() const noexcept { return m_ready; }
    const std::vector<Device>& devices() const noexcept { return m_devices; }

    Reading read(const Device& d) const;

private:
    void load();
};

} // namespace detail
} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_NVML_HPP
