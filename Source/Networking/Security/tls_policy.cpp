#include "fizmo_library.hpp"
#include "tls_policy.hpp"
#include "../../Random/random_std_int.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace networking {
namespace security {

const char* protocol_preset_to_string(ProtocolPreset p) noexcept {
    switch (p) {
        case ProtocolPreset::Modern:     return "Modern";
        case ProtocolPreset::Compatible: return "Compatible";
        case ProtocolPreset::Legacy:     return "Legacy";
    }

    return "Unknown";
}

PresetSettings preset_settings(ProtocolPreset preset) {
    switch (preset) {
        case ProtocolPreset::Modern:
            return { TLSVersion::TLS_1_3, TLSVersion::TLS_1_3, CipherList::tls13_only(), false };
        case ProtocolPreset::Compatible:
            return { TLSVersion::TLS_1_2, TLSVersion::TLS_1_3, CipherList::default_secure_list(), false };
        case ProtocolPreset::Legacy:
        default:
            return { TLSVersion::TLS_1_0, TLSVersion::TLS_1_3, CipherList::compatible_list(), true };
    }
}

std::chrono::milliseconds Backoff::next_delay() {
    const double base = static_cast<double>(m_policy.initial.count()) * std::pow(m_policy.multiplier, static_cast<double>(m_attempt));
    double capped = std::min(base, static_cast<double>(m_policy.maximum.count()));

    if (m_policy.jitter > 0.0) {
        const double u = static_cast<double>(unsecure_random_int<std::uint32_t>()) / 4294967295.0;
        capped *= 1.0 + m_policy.jitter * (2.0 * u - 1.0);
    }

    ++m_attempt;
    return std::chrono::milliseconds(static_cast<long long>(std::max(0.0, capped)));
}

void HostFailureTracker::configure(std::uint32_t max_failures, std::chrono::seconds blacklist) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max = max_failures;
    m_blacklist = blacklist;
}

void HostFailureTracker::record_failure(const std::string& host) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& e = m_hosts[host];
    ++e.failures;
    if (m_max != 0 && e.failures >= m_max) e.until = std::chrono::steady_clock::now() + m_blacklist;
}

void HostFailureTracker::record_success(const std::string& host) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hosts.erase(host);
}

bool HostFailureTracker::is_blacklisted(const std::string& host) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_hosts.find(host);
    if (it == m_hosts.end() || m_max == 0 || it->second.failures < m_max) return false;
    return std::chrono::steady_clock::now() < it->second.until;
}

std::uint32_t HostFailureTracker::failures(const std::string& host) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_hosts.find(host);
    return it == m_hosts.end() ? 0 : it->second.failures;
}

void HostFailureTracker::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hosts.clear();
}

} // namespace security
} // namespace networking
} // namespace fizmo
