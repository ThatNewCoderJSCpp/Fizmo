#ifndef FIZMO_SSL_SOCKET_HPP
#define FIZMO_SSL_SOCKET_HPP

#include "tls_context.hpp"
#include "tls_handshake.hpp"
#include "certificate_verifier.hpp"
#include "certificate.hpp"
#include "certificate_chain.hpp"
#include "../Core/tcp_socket.hpp"
#include "../Core/buffer.hpp"
#include "../Core/socket_error.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <memory>
#include <functional>
#include <algorithm>

namespace fizmo {
namespace networking {
namespace security {

class SSLSocket {
public:
    explicit SSLSocket(TLSContext& ctx, core::AddressFamily family = core::AddressFamily::IPv4) noexcept : m_ctx(ctx), m_socket(family), m_handshake_done(false), m_shutdown_sent(false), m_peer_closed(false) {}

    ~SSLSocket() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        close_unlocked();
    }

    SSLSocket(const SSLSocket&)            = delete;
    SSLSocket& operator=(const SSLSocket&) = delete;

public:
    bool connect(const core::NetworkAddress& addr, const std::string& hostname = {}) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_socket.connect(addr)) return false;
        m_hostname  = hostname.empty() ? addr.host() : hostname;
        m_handshake = std::make_unique<TLSHandshake>(m_ctx, HandshakeRole::Client, m_hostname);
        m_handshake->set_verifier(m_verifier);
        return run_handshake_unlocked();
    }

    bool accept(std::unique_ptr<core::TCPSocket> raw_conn) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!raw_conn) return false;
        m_socket    = std::move(*raw_conn);
        m_handshake = std::make_unique<TLSHandshake>(m_ctx, HandshakeRole::Server);
        m_handshake->set_verifier(m_verifier);
        return run_handshake_unlocked();
    }

public:
    int send(const void* data, std::size_t len) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !m_handshake || !data || len == 0) return -1;
        std::vector<std::uint8_t> wire;

        const int accepted = m_handshake->session().encrypt(
            static_cast<const std::uint8_t*>(data), len, wire
        );

        if (accepted <= 0) return -1;
        if (send_raw(wire) <= 0) return -1;
        return accepted;
    }

    int send_string(const std::string& str) noexcept { return send(str.data(), str.size()); }

    int receive(void* buffer, std::size_t len) noexcept {
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

            switch (io.status) {
                case detail::IOStatus::Ok:
                    if (!io.plaintext.empty()) {
                        m_plaintext = std::move(io.plaintext);
                        return drain_plaintext(buffer, len);
                    }
                    break;

                case detail::IOStatus::Closed:
                    m_peer_closed = true;
                    return 0;

                case detail::IOStatus::Renegotiate:
                    if (!renegotiate_unlocked()) return -1;
                    break;

                case detail::IOStatus::NeedMoreData:
                    break;

                case detail::IOStatus::Failed:
                default:
                    return -1;
            }

            const int n = m_socket.receive(read_buf.data(), read_buf.size());

            if (n == 0) {
                m_peer_closed = true;
                return 0;
            }

            if (n < 0) return n;
            m_cipher.insert(m_cipher.end(), read_buf.begin(), read_buf.begin() + n);
        }
    }

    bool receive_string(std::string& out, std::size_t max_len = 4096) noexcept {
        out.resize(max_len);
        const int n = receive(&out[0], max_len);
        if (n <= 0) { out.clear(); return false; }
        out.resize(static_cast<std::size_t>(n));
        return true;
    }

public:
    void close() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        close_unlocked();
    }

    void graceful_close(unsigned int linger_ms = 2000) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        send_close_notify_unlocked();
        m_socket.graceful_close(linger_ms);
        reset_state();
    }

    bool shutdown(core::ShutdownMode mode = core::ShutdownMode::Both) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        send_close_notify_unlocked();
        return m_socket.shutdown(mode);
    }

public:
    bool is_handshake_complete() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_handshake_done;
    }

    void set_verifier(std::shared_ptr<CertificateVerifier> v) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_verifier = std::move(v);
    }

    bool peer_certificate(Certificate& out) const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !m_handshake) return false;
        std::vector<std::vector<std::uint8_t>> ders;
        if (!m_handshake->session().peer_chain(ders) || ders.empty()) return false;
        return out.load_from_der(ders.front());
    }

    bool peer_chain(CertificateChain& out) const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !m_handshake) return false;
        std::vector<std::vector<std::uint8_t>> ders;
        if (!m_handshake->session().peer_chain(ders) || ders.empty()) return false;
        out = CertificateChain::from_der_list(ders);
        return !out.empty();
    }

    std::string negotiated_cipher() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !m_handshake) return {};
        return m_handshake->session().cipher_name();
    }

    TLSVersion negotiated_protocol_version() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !m_handshake) return TLSVersion::TLS_1_2;
        return m_handshake->session().protocol_version();
    }

public:
    core::TCPSocket&       socket()       noexcept { return m_socket; }
    const core::TCPSocket& socket() const noexcept { return m_socket; }

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

    std::string to_string() const {
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
        }

        s += ']';
        return s;
    }

private:
    mutable std::mutex                   m_mutex;
    TLSContext&                          m_ctx;
    core::TCPSocket                      m_socket;
    std::string                          m_hostname;
    std::unique_ptr<TLSHandshake>        m_handshake;
    std::shared_ptr<CertificateVerifier> m_verifier;
    bool                                 m_handshake_done;
    bool                                 m_shutdown_sent;
    bool                                 m_peer_closed;

    std::vector<std::uint8_t>            m_plaintext;
    std::vector<std::uint8_t>            m_cipher;   

private:
    bool run_handshake_unlocked() noexcept {
        HandshakeResult hr = m_handshake->start();
        if (hr.has_failed()) return false;
        if (hr.has_output() && send_raw(hr.output) <= 0) return false;
        if (!hr.extra.empty()) { m_cipher.insert(m_cipher.end(), hr.extra.begin(), hr.extra.end()); }
        std::vector<std::uint8_t> buf(16384);

        while (!hr.is_complete()) {
            const int n = m_socket.receive(buf.data(), buf.size());
            if (n <= 0) return false;
            hr = m_handshake->step(buf.data(), static_cast<std::size_t>(n));
            if (hr.has_failed()) return false;
            if (hr.has_output() && send_raw(hr.output) <= 0) return false;
            if (!hr.extra.empty()) { m_cipher.insert(m_cipher.end(), hr.extra.begin(), hr.extra.end()); }
        }

        m_handshake_done = true;
        return true;
    }

    bool renegotiate_unlocked() noexcept {
        if (!m_handshake) return false;
        m_handshake_done = false;

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

    int drain_plaintext(void* out, std::size_t len) noexcept {
        const std::size_t available = std::min(len, m_plaintext.size());
        std::memcpy(out, m_plaintext.data(), available);

        if (available == m_plaintext.size()) {
            m_plaintext.clear();
        } else {
            m_plaintext.erase(m_plaintext.begin(), m_plaintext.begin() + static_cast<std::ptrdiff_t>(available));
        }

        return static_cast<int>(available);
    }

    int send_raw(const std::vector<std::uint8_t>& data) noexcept {
        std::size_t sent = 0;

        while (sent < data.size()) {
            const int n = m_socket.send(data.data() + sent, data.size() - sent);
            if (n <= 0) return n;
            sent += static_cast<std::size_t>(n);
        }

        return static_cast<int>(sent);
    }

    void send_close_notify_unlocked() noexcept {
        if (!m_handshake_done || m_shutdown_sent || !m_handshake) return;
        m_shutdown_sent = true;
        std::vector<std::uint8_t> out;
        if (m_handshake->session().close_notify(out) && !out.empty()) { send_raw(out); }
    }

    void close_unlocked() noexcept {
        send_close_notify_unlocked();
        m_socket.close();
        reset_state();
    }

    void reset_state() noexcept {
        m_handshake.reset();
        m_handshake_done = false;
        m_shutdown_sent  = false;
        m_peer_closed    = false;
        m_plaintext.clear();
        m_cipher.clear();
    }
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SSL_SOCKET_HPP