#ifndef FIZMO_TLS_POLICY_HPP
#define FIZMO_TLS_POLICY_HPP

#include "Backend/tls_types.hpp"
#include "cipher_suites.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fizmo {
namespace networking {
namespace security {

enum class ProtocolPreset : std::uint8_t {
    Modern,
    Compatible,
    Legacy
};

const char* protocol_preset_to_string(ProtocolPreset p) noexcept;

struct PresetSettings {
    TLSVersion min_version;
    TLSVersion max_version;
    CipherList ciphers;
    bool       insecure;
};

PresetSettings preset_settings(ProtocolPreset preset);

struct BackoffPolicy {
    std::chrono::milliseconds initial    = std::chrono::milliseconds(100);
    std::chrono::milliseconds maximum    = std::chrono::milliseconds(30000);
    double                    multiplier = 2.0;
    double                    jitter     = 0.2;
    std::uint32_t             max_attempts = 0;
};

class Backoff {
public:
    explicit Backoff(BackoffPolicy policy = BackoffPolicy()) noexcept : m_policy(policy) {}

    std::chrono::milliseconds next_delay();
    void reset() noexcept { m_attempt = 0; }
    std::uint32_t attempts() const noexcept { return m_attempt; }
    bool exhausted() const noexcept { return m_policy.max_attempts != 0 && m_attempt >= m_policy.max_attempts; }
    const BackoffPolicy& policy() const noexcept { return m_policy; }

private:
    BackoffPolicy m_policy;
    std::uint32_t m_attempt = 0;
};

struct TLSPolicy {
    std::chrono::milliseconds handshake_timeout       = std::chrono::seconds(10);
    std::chrono::milliseconds close_notify_timeout    = std::chrono::seconds(5);
    std::chrono::milliseconds idle_timeout            = std::chrono::milliseconds(0);
    std::chrono::seconds      session_lifetime        = std::chrono::hours(2);
    std::size_t               max_cached_sessions     = 1024;
    std::uint32_t             max_handshake_failures  = 5;
    std::chrono::seconds      failure_blacklist_time  = std::chrono::seconds(60);
    std::chrono::seconds      key_update_interval     = std::chrono::seconds(0);
    std::uint64_t             key_update_bytes        = 0;
    std::size_t               max_pending_handshakes  = 256;
    BackoffPolicy             reconnect;
};

class HostFailureTracker {
public:
    explicit HostFailureTracker(std::uint32_t max_failures = 5, std::chrono::seconds blacklist = std::chrono::seconds(60)) noexcept
        : m_max(max_failures), m_blacklist(blacklist) {}

    void configure(std::uint32_t max_failures, std::chrono::seconds blacklist) noexcept;
    void record_failure(const std::string& host);
    void record_success(const std::string& host);
    bool is_blacklisted(const std::string& host) const;
    std::uint32_t failures(const std::string& host) const;
    void clear();

private:
    struct Entry {
        std::uint32_t                         failures = 0;
        std::chrono::steady_clock::time_point until;
    };

    std::uint32_t                          m_max;
    std::chrono::seconds                   m_blacklist;
    mutable std::mutex                     m_mutex;
    std::unordered_map<std::string, Entry> m_hosts;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_POLICY_HPP
