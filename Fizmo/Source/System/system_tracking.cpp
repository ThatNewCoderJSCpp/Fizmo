#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "system_tracking.hpp"

namespace fizmo {
namespace system {

SystemTracking::SystemTracking(std::chrono::milliseconds interval, std::size_t history) : m_cpu(interval, history), m_ram(interval, history), m_storage(interval, history), m_network(interval, history), m_gpu(interval, history) {}

auto SystemTracking::start() -> bool {
        bool ok = m_cpu.start();
        ok = m_ram.start() && ok;
        ok = m_storage.start() && ok;
        ok = m_network.start() && ok;
        ok = m_gpu.start() && ok;
        return ok;
    }

auto SystemTracking::running() const -> bool {
        return m_cpu.running() || m_ram.running() || m_storage.running() || m_network.running() || m_gpu.running();
    }

auto SystemTracking::set_interval(std::chrono::milliseconds interval) noexcept -> void {
        m_cpu.set_interval(interval);
        m_ram.set_interval(interval);
        m_storage.set_interval(interval);
        m_network.set_interval(interval);
        m_gpu.set_interval(interval);
    }

auto SystemTracking::report() const -> std::string {
        return m_cpu.report() + m_ram.report() + m_gpu.report() + m_storage.report() + m_network.report();
    }

} // namespace system
} // namespace fizmo
