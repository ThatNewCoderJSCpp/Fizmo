#ifndef FIZMO_SSL_SOCKET_HPP
#define FIZMO_SSL_SOCKET_HPP

#include "tls_context.hpp"
#include "tls_handshake.hpp"
#include "certificate_verifier.hpp"
#include "certificate.hpp"
#include "certificate_chain.hpp"
#include "ssl_error.hpp"
#include "ssl_socket_event.hpp"
#include "tls_metrics.hpp"
#include "../Core/tcp_socket.hpp"
#include "../Core/buffer.hpp"
#include "../Core/socket_error.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <mutex>
#include <memory>
#include <functional>
#include <algorithm>

namespace fizmo {
namespace networking {
namespace security {

enum class TLSShutdownMode : std::uint8_t {
    Graceful,
    Unilateral,
    Immediate
};

class SSLSocket {
public:
    explicit SSLSocket(TLSContext& ctx, core::AddressFamily family = core::AddressFamily::IPv4) noexcept;
    ~SSLSocket() noexcept;

    SSLSocket(const SSLSocket&)            = delete;
    SSLSocket& operator=(const SSLSocket&) = delete;

public:
    bool connect(const core::NetworkAddress& addr, const std::string& hostname = {}) noexcept;
    bool accept(std::unique_ptr<core::TCPSocket> raw_conn) noexcept;

public:
    int send(const void* data, std::size_t len) noexcept;
    int send_string(const std::string& str) noexcept { return send(str.data(), str.size()); }
    bool send_all(const void* data, std::size_t len) noexcept;
    int receive(void* buffer, std::size_t len) noexcept;
    bool receive_string(std::string& out, std::size_t max_len = 4096) noexcept;

public:
    void close() noexcept { close(TLSShutdownMode::Unilateral); }
    bool close(TLSShutdownMode mode, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) noexcept;
    void graceful_close(unsigned int linger_ms = 2000) noexcept;
    bool shutdown(core::ShutdownMode mode = core::ShutdownMode::Both) noexcept;
    bool shutdown_send() noexcept;

public:
    bool is_handshake_complete() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_handshake_done;
    }
    bool peer_closed() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_peer_closed;
    }
    std::size_t pending() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_plaintext.size();
    }
    void set_verifier(std::shared_ptr<CertificateVerifier> v) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_verifier = std::move(v);
    }
    void set_event_handler(std::shared_ptr<SSLEventHandler> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events_handler = std::move(handler);
    }
    void set_session_key(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_session_key = key;
    }

    bool peer_certificate(Certificate& out) const noexcept;
    bool peer_chain(CertificateChain& out) const noexcept;
    std::string negotiated_cipher() const noexcept;
    TLSVersion negotiated_protocol_version() const noexcept;
    std::string negotiated_alpn() const noexcept;
    std::string requested_server_name() const noexcept;
    bool session_resumed() const noexcept;
    bool export_keying_material(const std::string& label, const std::vector<std::uint8_t>& context, std::size_t length, std::vector<std::uint8_t>& out, bool use_context = true) const noexcept;
    bool channel_binding(ChannelBindingType type, std::vector<std::uint8_t>& out) const noexcept;
    std::vector<std::uint8_t> ocsp_response() const noexcept;
    TLSAlert last_alert() const noexcept;
    SSLError last_error() const noexcept;
    TLSMetrics metrics() const noexcept;
    bool key_update(bool request_peer = false) noexcept;
    bool request_client_certificate() noexcept;

public:
    core::TCPSocket&       socket()       noexcept { return m_socket; }
    const core::TCPSocket& socket() const noexcept { return m_socket; }
    core::detail::native_handle_t native_handle() const noexcept { return m_socket.get_raw_socket(); }
    TLSContext& context() noexcept { return m_ctx; }

    void set_connect_timeout(unsigned int ms) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_socket.set_connect_timeout(ms);
    }
    void set_send_timeout(unsigned int ms) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_socket.set_send_timeout(ms);
    }
    void set_receive_timeout(unsigned int ms) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_socket.set_receive_timeout(ms);
    }
    std::string to_string() const;

private:
    mutable std::mutex                   m_mutex;
    TLSContext&                          m_ctx;
    core::TCPSocket                      m_socket;
    std::string                          m_hostname;
    std::string                          m_session_key;
    std::unique_ptr<TLSHandshake>        m_handshake;
    std::shared_ptr<CertificateVerifier> m_verifier;
    std::shared_ptr<SSLEventHandler>     m_events_handler;
    std::vector<SSLEvent>                m_events;
    bool                                 m_handshake_done;
    bool                                 m_shutdown_sent;
    bool                                 m_peer_closed;
    std::vector<std::uint8_t>            m_plaintext;
    std::vector<std::uint8_t>            m_cipher;
    SSLError                             m_last_error;
    TLSMetrics                           m_metrics;
    std::shared_ptr<TLSMetricsCollector> m_collector;

private:
    bool run_handshake_unlocked() noexcept;
    bool renegotiate_unlocked() noexcept;
    int drain_plaintext(void* out, std::size_t len) noexcept;
    int send_raw(const std::vector<std::uint8_t>& data) noexcept;
    void send_close_notify_unlocked() noexcept;
    bool wait_close_notify_unlocked(std::chrono::milliseconds timeout) noexcept;
    void handshake_failed_unlocked(const std::string& fallback, HandshakeError err = HandshakeError::InternalError) noexcept;
    void note_alert_unlocked() noexcept;
    void queue_event(SSLEventType type, std::size_t bytes = 0) noexcept;
    void flush_events() noexcept;
    void close_unlocked(TLSShutdownMode mode, std::chrono::milliseconds timeout) noexcept;
    void reset_state() noexcept;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SSL_SOCKET_HPP
