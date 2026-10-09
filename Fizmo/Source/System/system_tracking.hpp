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
        : m_cpu(interval, history), m_ram(interval, history), m_storage(interval, history), m_network(interval, history), m_gpu(interval, history) {}

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

    bool start() {
        bool ok = m_cpu.start();
        ok = m_ram.start() && ok;
        ok = m_storage.start() && ok;
        ok = m_network.start() && ok;
        ok = m_gpu.start() && ok;
        return ok;
    }

    void stop() noexcept {
        m_cpu.stop();
        m_ram.stop();
        m_storage.stop();
        m_network.stop();
        m_gpu.stop();
    }

    bool running() const {
        return m_cpu.running() || m_ram.running() || m_storage.running() || m_network.running() || m_gpu.running();
    }

    void set_interval(std::chrono::milliseconds interval) noexcept {
        m_cpu.set_interval(interval);
        m_ram.set_interval(interval);
        m_storage.set_interval(interval);
        m_network.set_interval(interval);
        m_gpu.set_interval(interval);
    }

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

    std::string report() const {
        return m_cpu.report() + m_ram.report() + m_gpu.report() + m_storage.report() + m_network.report();
    }
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_SYSTEM_TRACKING_HPP
