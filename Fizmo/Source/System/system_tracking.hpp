#ifndef FIZMO_SYSTEM_SYSTEM_TRACKING_HPP
#define FIZMO_SYSTEM_SYSTEM_TRACKING_HPP

#include "cpu_tracking.hpp"
#include "ram_tracking.hpp"
#include "storage_tracking.hpp"
#include "network_tracking.hpp"
#include "gpu_tracking.hpp"

namespace fizmo {
namespace system {

class SystemTracking {
private:
    CPUTracking     m_cpu;
    RAMTracking     m_ram;
    StorageTracking m_storage;
    NetworkTracking m_network;
    GPUTracking     m_gpu;

public:
    explicit SystemTracking(std::chrono::milliseconds interval = std::chrono::milliseconds(1000), std::size_t history = 240)
;

    SystemTracking(const SystemTracking&) = delete;
    SystemTracking& operator=(const SystemTracking&) = delete;

    CPUTracking&     cpu()     noexcept { return m_cpu; }
    RAMTracking&     ram()     noexcept { return m_ram; }
    StorageTracking& storage() noexcept { return m_storage; }
    NetworkTracking& network() noexcept { return m_network; }
    GPUTracking&     gpu()     noexcept { return m_gpu; }

    const CPUTracking&     cpu()     const noexcept { return m_cpu; }
    const RAMTracking&     ram()     const noexcept { return m_ram; }
    const StorageTracking& storage() const noexcept { return m_storage; }
    const NetworkTracking& network() const noexcept { return m_network; }
    const GPUTracking&     gpu()     const noexcept { return m_gpu; }

    bool start();

    void stop() noexcept {
        m_cpu.stop();
        m_ram.stop();
        m_storage.stop();
        m_network.stop();
        m_gpu.stop();
    }

    bool running() const;

    void set_interval(std::chrono::milliseconds interval) noexcept;

    template <typename Target>
    void attach(Target& target) {
        m_cpu.attach(target);
        m_ram.attach(target);
        m_storage.attach(target);
        m_network.attach(target);
        m_gpu.attach(target);
    }

    void detach() noexcept {
        m_cpu.detach();
        m_ram.detach();
        m_storage.detach();
        m_network.detach();
        m_gpu.detach();
    }

    std::string report() const;
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_SYSTEM_TRACKING_HPP
