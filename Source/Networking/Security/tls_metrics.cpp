#include "fizmo_library.hpp"
#include "tls_metrics.hpp"

#include <sstream>

namespace fizmo {
namespace networking {
namespace security {

void TLSMetrics::finish_handshake(bool resumed) noexcept {
    handshake_finished = clock::now();
    handshake_duration = std::chrono::duration_cast<std::chrono::microseconds>(handshake_finished - handshake_started);
    handshake_complete = true;
    session_resumed = resumed;
}

std::string TLSMetrics::to_string() const {
    std::ostringstream os;
    os << "TLSMetrics[" << (handshake_complete ? "established" : "pending")
       << " handshake=" << (handshake_duration.count() / 1000.0) << "ms"
       << (session_resumed ? " resumed" : "")
       << " proto=" << protocol << " cipher=" << cipher;
    if (!alpn.empty()) os << " alpn=" << alpn;
    os << " enc=" << bytes_encrypted << " dec=" << bytes_decrypted
       << " tx=" << bytes_sent << " rx=" << bytes_received
       << " reneg=" << renegotiations << " keyupd=" << key_updates
       << " alerts=" << alerts_sent << "/" << alerts_received << "]";
    return os.str();
}

std::string TLSMetricsSnapshot::to_string() const {
    std::ostringstream os;
    os << "TLSMetricsSnapshot[handshakes=" << handshakes_completed << "/" << handshakes_started
       << " failed=" << handshakes_failed << " resumed=" << sessions_resumed
       << " avg=" << average_handshake_ms << "ms max=" << max_handshake_ms << "ms"
       << " active=" << active_connections
       << " enc=" << bytes_encrypted << " dec=" << bytes_decrypted
       << " tx=" << bytes_sent << " rx=" << bytes_received
       << " alerts=" << alerts_sent << "/" << alerts_received << "]";
    return os.str();
}

void TLSMetricsCollector::on_handshake_completed(const TLSMetrics& m) {
    ++m_completed;
    ++m_active;
    if (m.session_resumed) ++m_resumed;
    const double ms = static_cast<double>(m.handshake_duration.count()) / 1000.0;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_total_handshake_ms += ms;
    if (ms > m_max_handshake_ms) m_max_handshake_ms = ms;
    if (!m.protocol.empty()) ++m_protocols[m.protocol];
    if (!m.cipher.empty()) ++m_ciphers[m.cipher];
}

void TLSMetricsCollector::on_handshake_failed(VerifyError reason) {
    ++m_failed;
    if (reason == VerifyError::None) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_verify_failures[verify_error_to_string(reason)];
}

void TLSMetricsCollector::on_connection_closed(const TLSMetrics& m) noexcept {
    if (m.handshake_complete && m_active.load() > 0) --m_active;
}

void TLSMetricsCollector::add_traffic(std::uint64_t encrypted, std::uint64_t decrypted, std::uint64_t sent, std::uint64_t received) noexcept {
    m_encrypted += encrypted;
    m_decrypted += decrypted;
    m_sent += sent;
    m_received += received;
}

TLSMetricsSnapshot TLSMetricsCollector::snapshot() const {
    TLSMetricsSnapshot s;
    s.handshakes_started   = m_started.load();
    s.handshakes_completed = m_completed.load();
    s.handshakes_failed    = m_failed.load();
    s.sessions_resumed     = m_resumed.load();
    s.bytes_encrypted      = m_encrypted.load();
    s.bytes_decrypted      = m_decrypted.load();
    s.bytes_sent           = m_sent.load();
    s.bytes_received       = m_received.load();
    s.renegotiations       = m_renegotiations.load();
    s.alerts_sent          = m_alerts_sent.load();
    s.alerts_received      = m_alerts_received.load();
    s.active_connections   = m_active.load();
    s.resumption_rate      = s.handshakes_completed ? static_cast<double>(s.sessions_resumed) / static_cast<double>(s.handshakes_completed) : 0.0;
    std::lock_guard<std::mutex> lock(m_mutex);
    s.average_handshake_ms = s.handshakes_completed ? m_total_handshake_ms / static_cast<double>(s.handshakes_completed) : 0.0;
    s.max_handshake_ms     = m_max_handshake_ms;
    s.verify_failures      = m_verify_failures;
    s.protocols            = m_protocols;
    s.ciphers              = m_ciphers;
    return s;
}

void TLSMetricsCollector::reset() {
    m_started = 0; m_completed = 0; m_failed = 0; m_resumed = 0;
    m_encrypted = 0; m_decrypted = 0; m_sent = 0; m_received = 0;
    m_renegotiations = 0; m_alerts_sent = 0; m_alerts_received = 0; m_active = 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_total_handshake_ms = 0.0;
    m_max_handshake_ms = 0.0;
    m_verify_failures.clear();
    m_protocols.clear();
    m_ciphers.clear();
}

} // namespace security
} // namespace networking
} // namespace fizmo
