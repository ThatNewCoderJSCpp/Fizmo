#include "fizmo_library.hpp"
#include "tls_stream.hpp"
#include "composed.hpp"

#include <array>
#include <deque>
#include <mutex>

namespace fizmo {
namespace networking {
namespace async {

using namespace security;

struct TlsStream::Impl : std::enable_shared_from_this<TlsStream::Impl> {
    using Lock = std::unique_lock<std::mutex>;
    using Waiter = std::pair<std::uint64_t, ErrorHandler>;

    IOContext&                           ctx;
    TLSContext&                          tls;
    TcpStream                            tcp;
    std::mutex                           m;
    std::unique_ptr<TLSHandshake>        hs;
    std::string                          hostname;
    std::string                          session_key;
    std::shared_ptr<CertificateVerifier> verifier;
    bool                                 established = false;
    bool                                 peer_closed = false;
    bool                                 close_sent  = false;
    bool                                 closed      = false;
    std::vector<std::uint8_t>            cipher_in;
    std::vector<std::uint8_t>            plain;
    std::size_t                          plain_off = 0;
    std::array<std::uint8_t, 16384>      rbuf{};
    bool                                 raw_read_active = false;
    ErrorHandler                         hs_handler;
    bool                                 reading  = false;
    bool                                 draining = false;
    MutableBuffer                        rd_buf;
    IoHandler                            rd_handler;
    std::deque<std::vector<std::uint8_t>> wq;
    bool                                 writing = false;
    std::uint64_t                        enqueued = 0;
    std::uint64_t                        flushed  = 0;
    std::deque<Waiter>                   waiters;
    std::error_code                      write_error;
    SSLError                             last_error;
    TLSMetrics                           metrics;
    std::shared_ptr<TLSMetricsCollector> collector;
    bool                                 counted_active = false;
    std::chrono::milliseconds            handshake_timeout{-1};

    Impl(IOContext& c, TLSContext& t) : ctx(c), tls(t), tcp(c) {}
    Impl(TcpStream&& s, TLSContext& t) : ctx(s.context()), tls(t), tcp(std::move(s)) {}

    void post_error(ErrorHandler h, std::error_code ec) {
        if (!h) return;
        ctx.post([h = std::move(h), ec]() mutable { h(ec); });
    }

    void post_io(IoHandler h, std::error_code ec, std::size_t n) {
        if (!h) return;
        ctx.post([h = std::move(h), ec, n]() mutable { h(ec, n); });
    }

    void enqueue_wire(std::vector<std::uint8_t> bytes, ErrorHandler done) {
        if (write_error) { post_error(std::move(done), write_error); return; }

        if (bytes.empty()) {
            if (done) waiters.emplace_back(enqueued, std::move(done));
            if (!writing) release_waiters({});
            return;
        }

        enqueued += bytes.size();
        wq.push_back(std::move(bytes));
        if (done) waiters.emplace_back(enqueued, std::move(done));
        if (!writing) start_write();
    }

    void release_waiters(std::error_code ec) {
        while (!waiters.empty() && (ec || waiters.front().first <= flushed)) {
            post_error(std::move(waiters.front().second), ec);
            waiters.pop_front();
        }
    }

    void start_write() {
        writing = true;
        auto self = shared_from_this();
        const auto& front = wq.front();
        async_write(tcp, ConstBuffer(front.data(), front.size()), [self](std::error_code ec, std::size_t n) { self->on_written(ec, n); });
    }

    void on_written(std::error_code ec, std::size_t n) {
        Lock lk(m);
        flushed += n;
        metrics.bytes_sent += n;
        if (collector) collector->add_traffic(0, 0, n, 0);
        if (!wq.empty()) wq.pop_front();

        if (ec) {
            write_error = ec;
            wq.clear();
            writing = false;
            release_waiters(ec);
            return;
        }

        release_waiters({});
        if (!wq.empty()) start_write();
        else writing = false;
    }

    void set_error_from_handshake(const HandshakeResult& hr) {
        if (!hs) return;
        const AlertInfo alert = hs->session().last_alert();

        if (!hs->verify_result().passed) {
            last_error = SSLError::from_verify(hs->verify_result());

            if (alert.present) {
                last_error.has_alert = true;
                last_error.alert = TLSAlert::from_info(alert);
            }
        } else {
            last_error = SSLError::from_handshake(hr.error, alert, hr.detail);
        }
    }

    std::error_code handshake_error_code() const {
        switch (last_error.code) {
            case SSLErrorCode::CertificateExpired:
            case SSLErrorCode::CertificateRevoked:
            case SSLErrorCode::CertificateUntrusted:
            case SSLErrorCode::CertificateInvalid:
            case SSLErrorCode::HostnameMismatch:
            case SSLErrorCode::PinViolation:
                return make_error_code(error::certificate_error);
            case SSLErrorCode::Timeout:
                return make_error_code(error::timed_out);
            default:
                return make_error_code(error::handshake_failed);
        }
    }

    void finish_handshake(std::error_code ec) {
        if (!hs_handler) return;
        ErrorHandler h = std::move(hs_handler);
        hs_handler = nullptr;
        post_error(std::move(h), ec);
    }

    void handle_hs(HandshakeResult hr) {
        auto self = shared_from_this();

        if (hr.has_failed()) {
            set_error_from_handshake(hr);
            const std::error_code ec = handshake_error_code();
            tcp.clear_timeout();

            if (collector) collector->on_handshake_failed(hs->verify_result().error);

            if (hr.has_output()) {
                enqueue_wire(std::move(hr.output), [self, ec](std::error_code) {
                    Lock lk(self->m);
                    self->finish_handshake(ec);
                });
            } else {
                finish_handshake(ec);
            }

            return;
        }

        if (!hr.extra.empty()) cipher_in.insert(cipher_in.end(), hr.extra.begin(), hr.extra.end());
        const bool complete = hr.is_complete();

        if (complete) {
            if (!established) {
                const TLSMetrics& hm = hs->metrics();
                metrics.handshake_started = hm.handshake_started;
                metrics.handshake_finished = hm.handshake_finished;
                metrics.handshake_duration = hm.handshake_duration;
                metrics.handshake_complete = true;
                metrics.session_resumed = hm.session_resumed;
                metrics.protocol = hm.protocol;
                metrics.cipher = hm.cipher;
                metrics.alpn = hm.alpn;
                if (collector && !counted_active) counted_active = true;
            }

            established = true;
            tcp.clear_timeout();
        }

        if (hr.has_output()) {
            if (complete) {
                enqueue_wire(std::move(hr.output), [self](std::error_code ec) {
                    Lock lk(self->m);
                    self->finish_handshake(ec);
                });
            } else {
                enqueue_wire(std::move(hr.output), [self](std::error_code ec) {
                    if (!ec) return;
                    Lock lk(self->m);
                    self->finish_handshake(ec);
                });
            }
        } else if (complete) {
            finish_handshake({});
        }

        if (!complete) read_raw_for_handshake();
    }

    void read_raw_for_handshake() {
        if (raw_read_active) return;
        raw_read_active = true;
        auto self = shared_from_this();
        tcp.async_read_some(MutableBuffer(rbuf.data(), rbuf.size()), [self](std::error_code ec, std::size_t n) { self->on_hs_read(ec, n); });
    }

    void on_hs_read(std::error_code ec, std::size_t n) {
        Lock lk(m);
        raw_read_active = false;

        if (ec) {
            last_error.code = ec == error::timed_out ? SSLErrorCode::Timeout : SSLErrorCode::IOError;
            last_error.detail = ec.message();
            if (collector) collector->on_handshake_failed();
            finish_handshake(ec == error::eof ? make_error_code(error::handshake_failed) : ec);
            return;
        }

        metrics.bytes_received += n;
        if (collector) collector->add_traffic(0, 0, 0, n);
        handle_hs(hs->step(rbuf.data(), n));
    }

    void start_handshake(HandshakeType type, ErrorHandler handler) {
        if (hs) { post_error(std::move(handler), make_error_code(error::already_open)); return; }
        if (!tcp.is_open()) { post_error(std::move(handler), make_error_code(error::not_connected)); return; }
        const HandshakeRole role = type == HandshakeType::Client ? HandshakeRole::Client : HandshakeRole::Server;
        hs.reset(new TLSHandshake(tls, role, hostname));
        hs->set_verifier(verifier);

        if (role == HandshakeRole::Client) {
            if (!session_key.empty()) hs->set_session_key(session_key);
            else if (!hostname.empty()) hs->set_session_key(TLSSessionCache::make_key(hostname, tcp.remote_endpoint().port()));
        }

        try {
            collector = tls.metrics();
            const auto timeout = handshake_timeout.count() >= 0 ? handshake_timeout : tls.policy().handshake_timeout;
            if (timeout.count() > 0) tcp.set_timeout(timeout);
        } catch (...) {}

        hs_handler = std::move(handler);
        handle_hs(hs->start());
    }

    void complete_read(std::error_code ec, std::size_t n) {
        IoHandler h = std::move(rd_handler);
        rd_handler = nullptr;
        reading = false;
        draining = false;
        post_io(std::move(h), ec, n);
    }

    std::size_t copy_plain() {
        const std::size_t avail = plain.size() - plain_off;
        const std::size_t n = std::min(avail, rd_buf.size());
        std::memcpy(rd_buf.data(), plain.data() + plain_off, n);
        plain_off += n;
        if (plain_off >= plain.size()) { plain.clear(); plain_off = 0; }
        return n;
    }

    void pump_read() {
        auto self = shared_from_this();

        for (;;) {
            if (closed) { complete_read(make_error_code(error::operation_aborted), 0); return; }
            security::detail::DecryptIO io = hs->session().decrypt(cipher_in.empty() ? nullptr : cipher_in.data(), cipher_in.size());
            cipher_in = std::move(io.leftover);
            if (!io.outgoing.empty()) enqueue_wire(std::move(io.outgoing), nullptr);

            switch (io.status) {
                case security::detail::IOStatus::Ok:
                    if (io.plaintext.empty()) continue;
                    metrics.bytes_decrypted += io.plaintext.size();
                    if (collector) collector->add_traffic(0, io.plaintext.size(), 0, 0);
                    if (draining) continue;
                    plain = std::move(io.plaintext);
                    plain_off = 0;
                    complete_read({}, copy_plain());
                    return;

                case security::detail::IOStatus::Closed:
                    peer_closed = true;
                    complete_read(make_error_code(error::eof), 0);
                    return;

                case security::detail::IOStatus::Renegotiate: {
                    ++metrics.renegotiations;
                    if (collector) collector->on_renegotiation();
                    std::vector<std::uint8_t> pending;
                    pending.swap(cipher_in);
                    hs_handler = [self](std::error_code ec) {
                        Lock lk(self->m);
                        if (ec) self->complete_read(ec, 0);
                        else self->pump_read();
                    };
                    handle_hs(hs->renegotiate(pending.empty() ? nullptr : pending.data(), pending.size()));
                    return;
                }

                case security::detail::IOStatus::NeedMoreData:
                    if (raw_read_active) return;
                    raw_read_active = true;
                    tcp.async_read_some(MutableBuffer(rbuf.data(), rbuf.size()), [self](std::error_code ec, std::size_t n) { self->on_app_read(ec, n); });
                    return;

                case security::detail::IOStatus::Failed:
                default: {
                    const AlertInfo a = hs->session().last_alert();
                    last_error = SSLError::from_handshake(HandshakeError::ProtocolError, a, "TLS record could not be decrypted");
                    if (last_error.code == SSLErrorCode::HandshakeFailed) last_error.code = SSLErrorCode::DecryptionFailed;
                    complete_read(make_error_code(error::tls_error), 0);
                    return;
                }
            }
        }
    }

    void on_app_read(std::error_code ec, std::size_t n) {
        Lock lk(m);
        raw_read_active = false;

        if (ec) {
            if (ec == error::eof && !peer_closed) {
                last_error.code = SSLErrorCode::PeerClosed;
                last_error.detail = "Connection closed without close_notify";
            }

            complete_read(ec, 0);
            return;
        }

        metrics.bytes_received += n;
        if (collector) collector->add_traffic(0, 0, 0, n);
        cipher_in.insert(cipher_in.end(), rbuf.begin(), rbuf.begin() + static_cast<std::ptrdiff_t>(n));
        pump_read();
    }

    void close() {
        if (closed) return;
        closed = true;
        if (collector && counted_active) collector->on_connection_closed(metrics);
        tcp.close();
    }
};

TlsStream::TlsStream(IOContext& ctx, TLSContext& tls) : m_impl(std::make_shared<Impl>(ctx, tls)) {}

TlsStream::TlsStream(TcpStream&& socket, TLSContext& tls) : m_impl(std::make_shared<Impl>(std::move(socket), tls)) {}

TlsStream::~TlsStream() {
    if (!m_impl) return;
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->close();
}

IOContext& TlsStream::context() const noexcept { return m_impl->ctx; }
TcpStream& TlsStream::next_layer() noexcept { return m_impl->tcp; }
const TcpStream& TlsStream::next_layer() const noexcept { return m_impl->tcp; }
TLSContext& TlsStream::tls_context() const noexcept { return m_impl->tls; }

void TlsStream::set_hostname(const std::string& hostname) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->hostname = hostname;
}

void TlsStream::set_session_key(const std::string& key) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->session_key = key;
}

void TlsStream::set_verifier(std::shared_ptr<CertificateVerifier> verifier) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->verifier = std::move(verifier);
}

void TlsStream::set_handshake_timeout(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->handshake_timeout = timeout;
}

void TlsStream::async_handshake(HandshakeType type, ErrorHandler handler) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->start_handshake(type, std::move(handler));
}

void TlsStream::async_connect(const Endpoint& endpoint, const std::string& hostname, ErrorHandler handler) {
    auto impl = m_impl;

    {
        std::lock_guard<std::mutex> lk(impl->m);
        if (!hostname.empty()) impl->hostname = hostname;
    }

    impl->tcp.async_connect(endpoint, [impl, h = std::move(handler)](std::error_code ec) mutable {
        std::lock_guard<std::mutex> lk(impl->m);
        if (ec) { impl->post_error(std::move(h), ec); return; }
        impl->start_handshake(HandshakeType::Client, std::move(h));
    });
}

void TlsStream::async_read_some(MutableBuffer buffer, IoHandler handler) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    Impl& s = *m_impl;
    if (s.reading) { s.post_io(std::move(handler), make_error_code(error::in_progress), 0); return; }
    if (!s.established) { s.post_io(std::move(handler), make_error_code(error::not_connected), 0); return; }
    if (buffer.size() == 0) { s.post_io(std::move(handler), {}, 0); return; }
    s.reading = true;
    s.rd_buf = buffer;
    s.rd_handler = std::move(handler);

    if (!s.plain.empty()) {
        s.complete_read({}, s.copy_plain());
        return;
    }

    if (s.peer_closed) {
        s.complete_read(make_error_code(error::eof), 0);
        return;
    }

    s.pump_read();
}

void TlsStream::async_write_some(ConstBuffer buffer, IoHandler handler) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    Impl& s = *m_impl;
    if (!s.established) { s.post_io(std::move(handler), make_error_code(error::not_connected), 0); return; }
    if (s.close_sent) { s.post_io(std::move(handler), make_error_code(error::shut_down), 0); return; }
    if (s.write_error) { s.post_io(std::move(handler), s.write_error, 0); return; }
    if (buffer.size() == 0) { s.post_io(std::move(handler), {}, 0); return; }
    const std::size_t chunk = std::min<std::size_t>(buffer.size(), 4 * s.hs->session().max_plaintext_chunk());
    std::vector<std::uint8_t> wire;
    const int accepted = s.hs->session().encrypt(static_cast<const std::uint8_t*>(buffer.data()), chunk, wire);

    if (accepted <= 0) {
        s.last_error.code = SSLErrorCode::InternalError;
        s.last_error.detail = "encrypt failed";
        s.post_io(std::move(handler), make_error_code(error::tls_error), 0);
        return;
    }

    s.metrics.bytes_encrypted += static_cast<std::uint64_t>(accepted);
    if (s.collector) s.collector->add_traffic(static_cast<std::uint64_t>(accepted), 0, 0, 0);
    const std::size_t n = static_cast<std::size_t>(accepted);
    s.enqueue_wire(std::move(wire), [h = std::move(handler), n](std::error_code ec) mutable { h(ec, ec ? 0 : n); });
}

void TlsStream::async_shutdown(ErrorHandler handler) {
    async_shutdown(TLSShutdownMode::Graceful, std::move(handler));
}

void TlsStream::async_shutdown(TLSShutdownMode mode, ErrorHandler handler) {
    auto impl = m_impl;
    std::lock_guard<std::mutex> lk(impl->m);

    if (mode == TLSShutdownMode::Immediate || !impl->established) {
        impl->close();
        impl->post_error(std::move(handler), {});
        return;
    }

    std::vector<std::uint8_t> out;

    if (!impl->close_sent) {
        impl->close_sent = true;
        impl->hs->session().close_notify(out);
    }

    impl->enqueue_wire(std::move(out), [impl, mode, h = std::move(handler)](std::error_code ec) mutable {
        std::lock_guard<std::mutex> lk2(impl->m);
        if (ec) { impl->post_error(std::move(h), ec); return; }

        if (mode == TLSShutdownMode::Unilateral || impl->peer_closed || impl->reading || impl->closed) {
            std::error_code ignored;
            impl->tcp.shutdown(ShutdownType::Send, ignored);
            impl->post_error(std::move(h), {});
            return;
        }

        try {
            const auto policy = impl->tls.policy();
            if (policy.close_notify_timeout.count() > 0) impl->tcp.set_timeout(policy.close_notify_timeout);
        } catch (...) {}

        static thread_local std::uint8_t scratch[1];
        impl->reading = true;
        impl->draining = true;
        impl->rd_buf = MutableBuffer(scratch, sizeof(scratch));
        impl->rd_handler = [impl, h = std::move(h)](std::error_code e, std::size_t) mutable {
            impl->tcp.clear_timeout();
            h(e == error::eof ? std::error_code() : e);
        };
        impl->pump_read();
    });
}

void TlsStream::async_key_update(bool request_peer, ErrorHandler handler) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    Impl& s = *m_impl;
    std::vector<std::uint8_t> out;

    if (!s.established || !s.hs->session().key_update(request_peer, out)) {
        s.post_error(std::move(handler), make_error_code(error::unsupported));
        return;
    }

    ++s.metrics.key_updates;
    s.enqueue_wire(std::move(out), std::move(handler));
}

void TlsStream::async_request_client_certificate(ErrorHandler handler) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    Impl& s = *m_impl;
    std::vector<std::uint8_t> out;

    if (!s.established || !s.hs->session().request_post_handshake_auth(out)) {
        s.post_error(std::move(handler), make_error_code(error::unsupported));
        return;
    }

    s.enqueue_wire(std::move(out), std::move(handler));
}

void TlsStream::close() {
    if (!m_impl) return;
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->close();
}

void TlsStream::cancel() {
    m_impl->tcp.cancel();
}

bool TlsStream::is_open() const noexcept {
    return m_impl && m_impl->tcp.is_open();
}

Endpoint TlsStream::local_endpoint() const { return m_impl->tcp.local_endpoint(); }
Endpoint TlsStream::remote_endpoint() const { return m_impl->tcp.remote_endpoint(); }
void TlsStream::set_timeout(std::chrono::milliseconds timeout) { m_impl->tcp.set_timeout(timeout); }
void TlsStream::clear_timeout() { m_impl->tcp.clear_timeout(); }

std::string TlsStream::description() const {
    std::string s = "tls://" + remote_endpoint().to_string();
    std::lock_guard<std::mutex> lk(m_impl->m);

    if (m_impl->established) {
        s += " ";
        s += tls_version_to_string(m_impl->hs->session().protocol_version());
        const std::string alpn = m_impl->hs->session().alpn_selected();
        if (!alpn.empty()) s += " " + alpn;
    }

    return s;
}

bool TlsStream::handshake_complete() const noexcept {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->established;
}

bool TlsStream::peer_closed() const noexcept {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->peer_closed;
}

std::string TlsStream::negotiated_alpn() const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->established ? m_impl->hs->session().alpn_selected() : std::string();
}

std::string TlsStream::negotiated_cipher() const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->established ? m_impl->hs->session().cipher_name() : std::string();
}

TLSVersion TlsStream::negotiated_protocol_version() const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->established ? m_impl->hs->session().protocol_version() : TLSVersion::TLS_1_2;
}

std::string TlsStream::requested_server_name() const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->hs ? m_impl->hs->requested_server_name() : std::string();
}

bool TlsStream::session_resumed() const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->established && m_impl->hs->session().session_reused();
}

bool TlsStream::peer_certificate(Certificate& out) const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    if (!m_impl->hs) return false;
    std::vector<std::vector<std::uint8_t>> ders;
    if (!m_impl->hs->session().peer_chain(ders) || ders.empty()) return false;
    return out.load_from_der(ders.front());
}

bool TlsStream::peer_chain(CertificateChain& out) const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    if (!m_impl->hs) return false;
    std::vector<std::vector<std::uint8_t>> ders;
    if (!m_impl->hs->session().peer_chain(ders) || ders.empty()) return false;
    out = CertificateChain::from_der_list(ders);
    return !out.empty();
}

bool TlsStream::export_keying_material(const std::string& label, const std::vector<std::uint8_t>& context, std::size_t length, std::vector<std::uint8_t>& out, bool use_context) const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->established && m_impl->hs->session().export_keying_material(label, context.data(), context.size(), use_context, length, out);
}

bool TlsStream::channel_binding(ChannelBindingType type, std::vector<std::uint8_t>& out) const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->established && m_impl->hs->session().channel_binding(type, out);
}

std::vector<std::uint8_t> TlsStream::ocsp_response() const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->hs ? m_impl->hs->session().ocsp_response() : std::vector<std::uint8_t>();
}

SSLError TlsStream::last_error() const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->last_error;
}

TLSMetrics TlsStream::metrics() const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->metrics;
}

} // namespace async
} // namespace networking
} // namespace fizmo
