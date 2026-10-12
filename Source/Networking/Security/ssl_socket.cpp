#include "fizmo_library.hpp"
#include "ssl_socket.hpp"

namespace fizmo {
namespace networking {
namespace security {

SSLSocket::SSLSocket(TLSContext& ctx, core::AddressFamily family) noexcept : m_ctx(ctx), m_socket(family), m_handshake_done(false), m_shutdown_sent(false), m_peer_closed(false) {}

SSLSocket::~SSLSocket() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    close_unlocked(TLSShutdownMode::Unilateral, std::chrono::milliseconds(0));
    m_events.clear();
}

void SSLSocket::queue_event(SSLEventType type, std::size_t bytes) noexcept {
    if (!m_events_handler) return;

    try {
        SSLEvent e;
        e.type = type;
        e.bytes = bytes;
        e.error = m_last_error;
        if (m_handshake) e.alert = TLSAlert::from_info(m_handshake->session().last_alert());
        m_events.push_back(std::move(e));
    } catch (...) {}
}

void SSLSocket::flush_events() noexcept {
    std::vector<SSLEvent> events;
    std::shared_ptr<SSLEventHandler> handler;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        events.swap(m_events);
        handler = m_events_handler;
    }

    if (!handler) return;

    for (const auto& e : events) {
        try { handler->dispatch(*this, e); } catch (...) {}
    }
}

void SSLSocket::handshake_failed_unlocked(const std::string& fallback, HandshakeError err) noexcept {
    AlertInfo alert;
    std::string detail = fallback;

    if (m_handshake) {
        alert = m_handshake->session().last_alert();
        if (!m_handshake->verify_result().passed) {
            m_last_error = SSLError::from_verify(m_handshake->verify_result());
            if (alert.present) { m_last_error.has_alert = true; m_last_error.alert = TLSAlert::from_info(alert); }
            queue_event(SSLEventType::HandshakeFailed);
            return;
        }
    }

    m_last_error = SSLError::from_handshake(err, alert, detail);
    queue_event(SSLEventType::HandshakeFailed);
}

bool SSLSocket::connect(const core::NetworkAddress& addr, const std::string& hostname) noexcept {
    bool ok = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_last_error = SSLError();

        if (!m_socket.connect(addr)) {
            m_last_error.code = SSLErrorCode::IOError;
            m_last_error.detail = "TCP connect failed";
        } else {
            m_hostname  = hostname.empty() ? addr.host() : hostname;
            m_handshake = std::make_unique<TLSHandshake>(m_ctx, HandshakeRole::Client, m_hostname);
            m_handshake->set_verifier(m_verifier);
            m_handshake->set_session_key(m_session_key.empty() ? TLSSessionCache::make_key(m_hostname, addr.port()) : m_session_key);
            ok = run_handshake_unlocked();
        }
    }

    flush_events();
    return ok;
}

bool SSLSocket::accept(std::unique_ptr<core::TCPSocket> raw_conn) noexcept {
    bool ok = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_last_error = SSLError();

        if (raw_conn) {
            m_socket    = std::move(*raw_conn);
            m_handshake = std::make_unique<TLSHandshake>(m_ctx, HandshakeRole::Server);
            m_handshake->set_verifier(m_verifier);
            ok = run_handshake_unlocked();
        }
    }

    flush_events();
    return ok;
}

int SSLSocket::send(const void* data, std::size_t len) noexcept {
    int result = -1;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !m_handshake || !data || len == 0 || m_shutdown_sent) return -1;
        std::vector<std::uint8_t> wire;

        const int accepted = m_handshake->session().encrypt(
            static_cast<const std::uint8_t*>(data), len, wire
        );

        if (accepted > 0 && send_raw(wire) > 0) {
            result = accepted;
            m_metrics.bytes_encrypted += static_cast<std::uint64_t>(accepted);
            m_metrics.bytes_sent += wire.size();
            if (m_collector) m_collector->add_traffic(static_cast<std::uint64_t>(accepted), 0, wire.size(), 0);
            queue_event(SSLEventType::SecureWrite, static_cast<std::size_t>(accepted));
        } else {
            m_last_error.code = SSLErrorCode::IOError;
            m_last_error.detail = "send failed";
        }
    }

    flush_events();
    return result;
}

bool SSLSocket::send_all(const void* data, std::size_t len) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::size_t done = 0;

    while (done < len) {
        const int n = send(p + done, len - done);
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }

    return true;
}

int SSLSocket::receive(void* buffer, std::size_t len) noexcept {
    int result = -1;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !m_handshake || !buffer || len == 0) return -1;
        if (!m_plaintext.empty()) return drain_plaintext(buffer, len);
        if (m_peer_closed) return 0;
        std::vector<std::uint8_t> read_buf(16384);

        for (;;) {
            detail::DecryptIO io = m_handshake->session().decrypt(
                m_cipher.empty() ? nullptr : m_cipher.data(),
                m_cipher.size()
            );

            m_cipher = std::move(io.leftover);
            if (!io.outgoing.empty()) { send_raw(io.outgoing); }
            bool done = false;

            switch (io.status) {
                case detail::IOStatus::Ok:
                    if (!io.plaintext.empty()) {
                        m_metrics.bytes_decrypted += io.plaintext.size();
                        if (m_collector) m_collector->add_traffic(0, io.plaintext.size(), 0, 0);
                        m_plaintext = std::move(io.plaintext);
                        result = drain_plaintext(buffer, len);
                        queue_event(SSLEventType::SecureRead, static_cast<std::size_t>(result));
                        done = true;
                    }
                    break;

                case detail::IOStatus::Closed:
                    m_peer_closed = true;
                    note_alert_unlocked();
                    queue_event(SSLEventType::PeerClosed);
                    result = 0;
                    done = true;
                    break;

                case detail::IOStatus::Renegotiate:
                    if (!renegotiate_unlocked()) { result = -1; done = true; }
                    else queue_event(SSLEventType::Renegotiation);
                    break;

                case detail::IOStatus::NeedMoreData:
                    break;

                case detail::IOStatus::Failed:
                default:
                    note_alert_unlocked();
                    m_last_error = SSLError::from_handshake(HandshakeError::ProtocolError, m_handshake->session().last_alert(), "decrypt failed");
                    if (m_last_error.code == SSLErrorCode::HandshakeFailed) m_last_error.code = SSLErrorCode::DecryptionFailed;
                    result = -1;
                    done = true;
                    break;
            }

            if (done) break;
            const int n = m_socket.receive(read_buf.data(), read_buf.size());

            if (n == 0) {
                m_peer_closed = true;
                queue_event(SSLEventType::PeerClosed);
                result = 0;
                break;
            }

            if (n < 0) {
                m_last_error.code = SSLErrorCode::IOError;
                m_last_error.detail = "receive failed";
                result = n;
                break;
            }

            m_metrics.bytes_received += static_cast<std::uint64_t>(n);
            if (m_collector) m_collector->add_traffic(0, 0, 0, static_cast<std::uint64_t>(n));
            m_cipher.insert(m_cipher.end(), read_buf.begin(), read_buf.begin() + n);
        }
    }

    flush_events();
    return result;
}

bool SSLSocket::receive_string(std::string& out, std::size_t max_len) noexcept {
    out.resize(max_len);
    const int n = receive(&out[0], max_len);
    if (n <= 0) { out.clear(); return false; }
    out.resize(static_cast<std::size_t>(n));
    return true;
}

void SSLSocket::note_alert_unlocked() noexcept {
    if (!m_handshake) return;
    const AlertInfo info = m_handshake->session().last_alert();

    if (info.present && info.received && info.description != 0) {
        ++m_metrics.alerts_received;
        if (m_collector) m_collector->on_alert(true);
        queue_event(SSLEventType::AlertReceived);
    }
}

bool SSLSocket::close(TLSShutdownMode mode, std::chrono::milliseconds timeout) noexcept {
    bool ok;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const bool had_session = m_handshake_done;
        close_unlocked(mode, timeout);
        ok = had_session ? (mode != TLSShutdownMode::Graceful || m_peer_closed) : true;
    }

    flush_events();
    return ok;
}

void SSLSocket::close_unlocked(TLSShutdownMode mode, std::chrono::milliseconds timeout) noexcept {
    const bool active = m_handshake_done;

    if (mode != TLSShutdownMode::Immediate) {
        send_close_notify_unlocked();
        if (mode == TLSShutdownMode::Graceful && active && !m_peer_closed) wait_close_notify_unlocked(timeout);
    }

    m_socket.close();
    if (active && m_collector) m_collector->on_connection_closed(m_metrics);
    if (active) queue_event(SSLEventType::Closed);
    reset_state();
}

bool SSLSocket::wait_close_notify_unlocked(std::chrono::milliseconds timeout) noexcept {
    if (!m_handshake) return false;
    m_socket.set_receive_timeout(static_cast<unsigned int>(timeout.count() > 0 ? timeout.count() : 1));
    std::vector<std::uint8_t> buf(16384);
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!m_peer_closed && std::chrono::steady_clock::now() < deadline) {
        detail::DecryptIO io = m_handshake->session().decrypt(m_cipher.empty() ? nullptr : m_cipher.data(), m_cipher.size());
        m_cipher = std::move(io.leftover);
        if (io.status == detail::IOStatus::Closed) { m_peer_closed = true; break; }
        if (io.status == detail::IOStatus::Failed) break;
        if (io.status == detail::IOStatus::Ok) continue;
        const int n = m_socket.receive(buf.data(), buf.size());
        if (n <= 0) { if (n == 0) m_peer_closed = true; break; }
        m_cipher.insert(m_cipher.end(), buf.begin(), buf.begin() + n);
    }

    return m_peer_closed;
}

void SSLSocket::graceful_close(unsigned int linger_ms) noexcept {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        send_close_notify_unlocked();
        const bool active = m_handshake_done;
        m_socket.graceful_close(linger_ms);
        if (active && m_collector) m_collector->on_connection_closed(m_metrics);
        if (active) queue_event(SSLEventType::Closed);
        reset_state();
    }

    flush_events();
}

bool SSLSocket::shutdown(core::ShutdownMode mode) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    send_close_notify_unlocked();
    return m_socket.shutdown(mode);
}

bool SSLSocket::shutdown_send() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake_done) return false;
    send_close_notify_unlocked();
    return true;
}

bool SSLSocket::peer_certificate(Certificate& out) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake_done || !m_handshake) return false;
    std::vector<std::vector<std::uint8_t>> ders;
    if (!m_handshake->session().peer_chain(ders) || ders.empty()) return false;
    return out.load_from_der(ders.front());
}

bool SSLSocket::peer_chain(CertificateChain& out) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake_done || !m_handshake) return false;
    std::vector<std::vector<std::uint8_t>> ders;
    if (!m_handshake->session().peer_chain(ders) || ders.empty()) return false;
    out = CertificateChain::from_der_list(ders);
    return !out.empty();
}

std::string SSLSocket::negotiated_cipher() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake_done || !m_handshake) return {};
    return m_handshake->session().cipher_name();
}

auto SSLSocket::negotiated_protocol_version() const noexcept -> TLSVersion {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake_done || !m_handshake) return TLSVersion::TLS_1_2;
    return m_handshake->session().protocol_version();
}

std::string SSLSocket::negotiated_alpn() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake_done || !m_handshake) return {};
    return m_handshake->session().alpn_selected();
}

std::string SSLSocket::requested_server_name() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake) return {};
    return m_handshake->requested_server_name();
}

bool SSLSocket::session_resumed() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_handshake_done && m_handshake && m_handshake->session().session_reused();
}

bool SSLSocket::export_keying_material(const std::string& label, const std::vector<std::uint8_t>& context, std::size_t length, std::vector<std::uint8_t>& out, bool use_context) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake_done || !m_handshake) return false;
    return m_handshake->session().export_keying_material(label, context.data(), context.size(), use_context, length, out);
}

bool SSLSocket::channel_binding(ChannelBindingType type, std::vector<std::uint8_t>& out) const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake_done || !m_handshake) return false;
    return m_handshake->session().channel_binding(type, out);
}

std::vector<std::uint8_t> SSLSocket::ocsp_response() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake) return {};
    return m_handshake->session().ocsp_response();
}

TLSAlert SSLSocket::last_alert() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake) return TLSAlert();
    return TLSAlert::from_info(m_handshake->session().last_alert());
}

SSLError SSLSocket::last_error() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_last_error;
}

TLSMetrics SSLSocket::metrics() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
}

bool SSLSocket::key_update(bool request_peer) noexcept {
    bool ok = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !m_handshake) return false;
        std::vector<std::uint8_t> out;

        if (m_handshake->session().key_update(request_peer, out) && send_raw(out) > 0) {
            ++m_metrics.key_updates;
            queue_event(SSLEventType::KeyUpdate);
            ok = true;
        }
    }

    flush_events();
    return ok;
}

bool SSLSocket::request_client_certificate() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handshake_done || !m_handshake) return false;
    std::vector<std::uint8_t> out;
    return m_handshake->session().request_post_handshake_auth(out) && send_raw(out) > 0;
}

std::string SSLSocket::to_string() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string s = "SSLSocket[";
    s += m_handshake_done ? "connected" : "not connected";

    if (!m_hostname.empty()) {
        s += " host=\"";
        s += m_hostname;
        s += '"';
    }

    if (m_handshake_done && m_handshake) {
        s += " proto=";
        s += tls_version_to_string(m_handshake->session().protocol_version());
        const std::string alpn = m_handshake->session().alpn_selected();
        if (!alpn.empty()) s += " alpn=" + alpn;
        if (m_handshake->session().session_reused()) s += " resumed";
    }

    s += ']';
    return s;
}

bool SSLSocket::run_handshake_unlocked() noexcept {
    try { m_collector = m_ctx.metrics(); } catch (...) {}
    HandshakeResult hr = m_handshake->start();

    if (hr.has_failed()) {
        if (hr.has_output()) send_raw(hr.output);
        handshake_failed_unlocked(hr.detail, hr.error);
        return false;
    }

    if (hr.has_output() && send_raw(hr.output) <= 0) { handshake_failed_unlocked("send failed"); return false; }
    if (!hr.extra.empty()) { m_cipher.insert(m_cipher.end(), hr.extra.begin(), hr.extra.end()); }
    std::vector<std::uint8_t> buf(16384);

    while (!hr.is_complete()) {
        const int n = m_socket.receive(buf.data(), buf.size());
        if (n <= 0) { handshake_failed_unlocked(n == 0 ? "Peer closed the connection during the handshake" : "receive failed"); return false; }
        m_metrics.bytes_received += static_cast<std::uint64_t>(n);
        hr = m_handshake->step(buf.data(), static_cast<std::size_t>(n));

        if (hr.has_failed()) {
            if (hr.has_output()) send_raw(hr.output);
            handshake_failed_unlocked(hr.detail, hr.error);
            return false;
        }

        if (hr.has_output() && send_raw(hr.output) <= 0) { handshake_failed_unlocked("send failed"); return false; }
        if (!hr.extra.empty()) { m_cipher.insert(m_cipher.end(), hr.extra.begin(), hr.extra.end()); }
    }

    const TLSMetrics& hm = m_handshake->metrics();
    m_metrics.handshake_started = hm.handshake_started;
    m_metrics.handshake_finished = hm.handshake_finished;
    m_metrics.handshake_duration = hm.handshake_duration;
    m_metrics.handshake_complete = true;
    m_metrics.session_resumed = hm.session_resumed;
    m_metrics.protocol = hm.protocol;
    m_metrics.cipher = hm.cipher;
    m_metrics.alpn = hm.alpn;
    m_handshake_done = true;
    queue_event(SSLEventType::HandshakeComplete);
    return true;
}

bool SSLSocket::renegotiate_unlocked() noexcept {
    if (!m_handshake) return false;
    m_handshake_done = false;
    ++m_metrics.renegotiations;
    if (m_collector) m_collector->on_renegotiation();

    HandshakeResult hr = m_handshake->renegotiate(
        m_cipher.empty() ? nullptr : m_cipher.data(),
        m_cipher.size()
    );

    m_cipher.clear();
    std::vector<std::uint8_t> buf(16384);

    while (!hr.is_complete()) {
        if (hr.has_failed()) return false;
        if (hr.has_output() && send_raw(hr.output) <= 0) return false;
        const int n = m_socket.receive(buf.data(), buf.size());
        if (n <= 0) return false;
        hr = m_handshake->step(buf.data(), static_cast<std::size_t>(n));
    }

    if (hr.has_output() && send_raw(hr.output) <= 0) return false;
    if (!hr.extra.empty()) { m_cipher.insert(m_cipher.end(), hr.extra.begin(), hr.extra.end()); }
    m_handshake_done = true;
    return true;
}

int SSLSocket::drain_plaintext(void* out, std::size_t len) noexcept {
    const std::size_t available = std::min(len, m_plaintext.size());
    std::memcpy(out, m_plaintext.data(), available);

    if (available == m_plaintext.size()) {
        m_plaintext.clear();
    } else {
        m_plaintext.erase(m_plaintext.begin(), m_plaintext.begin() + static_cast<std::ptrdiff_t>(available));
    }

    return static_cast<int>(available);
}

int SSLSocket::send_raw(const std::vector<std::uint8_t>& data) noexcept {
    std::size_t sent = 0;

    while (sent < data.size()) {
        const int n = m_socket.send(data.data() + sent, data.size() - sent);
        if (n <= 0) return n;
        sent += static_cast<std::size_t>(n);
    }

    return static_cast<int>(sent);
}

void SSLSocket::send_close_notify_unlocked() noexcept {
    if (!m_handshake_done || m_shutdown_sent || !m_handshake) return;
    m_shutdown_sent = true;
    std::vector<std::uint8_t> out;

    if (m_handshake->session().close_notify(out) && !out.empty()) {
        send_raw(out);
        ++m_metrics.alerts_sent;
    }
}

void SSLSocket::reset_state() noexcept {
    m_handshake.reset();
    m_handshake_done = false;
    m_shutdown_sent  = false;
    m_peer_closed    = false;
    m_plaintext.clear();
    m_cipher.clear();
}

} // namespace security
} // namespace networking
} // namespace fizmo
