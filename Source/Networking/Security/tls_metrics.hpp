#ifndef FIZMO_TLS_METRICS_HPP
#define FIZMO_TLS_METRICS_HPP

#include "Backend/tls_types.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace fizmo {
namespace networking {
namespace security {

struct TLSMetrics {
    using clock = std::chrono::steady_clock;

    clock::time_point         handshake_started{};
    clock::time_point         handshake_finished{};
    std::chrono::microseconds handshake_duration{0};
    bool                      handshake_complete = false;
    bool                      session_resumed    = false;
    std::uint64_t             bytes_encrypted    = 0;
    std::uint64_t             bytes_decrypted    = 0;
    std::uint64_t             bytes_sent         = 0;
    std::uint64_t             bytes_received     = 0;
    std::uint64_t             records_sent       = 0;
    std::uint64_t             records_received   = 0;
    std::uint32_t             renegotiations     = 0;
    std::uint32_t             key_updates        = 0;
    std::uint32_t             alerts_sent        = 0;
    std::uint32_t             alerts_received    = 0;
    std::string               protocol;
    std::string               cipher;
    std::string               alpn;

    void start_handshake() noexcept { handshake_started = clock::now(); }
    void finish_handshake(bool resumed) noexcept;
    std::string to_string() const;
};

struct TLSMetricsSnapshot {
    std::uint64_t handshakes_started   = 0;
    std::uint64_t handshakes_completed = 0;
    std::uint64_t handshakes_failed    = 0;
    std::uint64_t sessions_resumed     = 0;
    std::uint64_t bytes_encrypted      = 0;
    std::uint64_t bytes_decrypted      = 0;
    std::uint64_t bytes_sent           = 0;
    std::uint64_t bytes_received       = 0;
    std::uint64_t renegotiations       = 0;
    std::uint64_t alerts_sent          = 0;
    std::uint64_t alerts_received      = 0;
    std::uint64_t active_connections   = 0;
    double        average_handshake_ms = 0.0;
    double        max_handshake_ms     = 0.0;
    double        resumption_rate      = 0.0;
    std::map<std::string, std::uint64_t> verify_failures;
    std::map<std::string, std::uint64_t> protocols;
    std::map<std::string, std::uint64_t> ciphers;

    std::string to_string() const;
};

class TLSMetricsCollector {
public:
    void on_handshake_started() noexcept { ++m_started; }
    void on_handshake_completed(const TLSMetrics& m);
    void on_handshake_failed(VerifyError reason = VerifyError::None);
    void on_connection_closed(const TLSMetrics& m) noexcept;
    void on_alert(bool received) noexcept { if (received) ++m_alerts_received; else ++m_alerts_sent; }
    void add_traffic(std::uint64_t encrypted, std::uint64_t decrypted, std::uint64_t sent, std::uint64_t received) noexcept;
    void on_renegotiation() noexcept { ++m_renegotiations; }

    TLSMetricsSnapshot snapshot() const;
    void reset();

private:
    std::atomic<std::uint64_t> m_started{0};
    std::atomic<std::uint64_t> m_completed{0};
    std::atomic<std::uint64_t> m_failed{0};
    std::atomic<std::uint64_t> m_resumed{0};
    std::atomic<std::uint64_t> m_encrypted{0};
    std::atomic<std::uint64_t> m_decrypted{0};
    std::atomic<std::uint64_t> m_sent{0};
    std::atomic<std::uint64_t> m_received{0};
    std::atomic<std::uint64_t> m_renegotiations{0};
    std::atomic<std::uint64_t> m_alerts_sent{0};
    std::atomic<std::uint64_t> m_alerts_received{0};
    std::atomic<std::uint64_t> m_active{0};

    mutable std::mutex                   m_mutex;
    double                               m_total_handshake_ms = 0.0;
    double                               m_max_handshake_ms   = 0.0;
    std::map<std::string, std::uint64_t> m_verify_failures;
    std::map<std::string, std::uint64_t> m_protocols;
    std::map<std::string, std::uint64_t> m_ciphers;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_METRICS_HPP
