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
    explicit SSLSocket(TLSContext& ctx, core::AddressFamily family = core::AddressFamily::IPv4) noexcept;

    ~SSLSocket() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        close_unlocked();
    }

    SSLSocket(const SSLSocket&)            = delete;
    SSLSocket& operator=(const SSLSocket&) = delete;

public:
    bool connect(const core::NetworkAddress& addr, const std::string& hostname = {}) noexcept;

    bool accept(std::unique_ptr<core::TCPSocket> raw_conn) noexcept;

public:
    int send(const void* data, std::size_t len) noexcept;

    int send_string(const std::string& str) noexcept { return send(str.data(), str.size()); }

    int receive(void* buffer, std::size_t len) noexcept;

    bool receive_string(std::string& out, std::size_t max_len = 4096) noexcept;

public:
    void close() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        close_unlocked();
    }

    void graceful_close(unsigned int linger_ms = 2000) noexcept;

    bool shutdown(core::ShutdownMode mode = core::ShutdownMode::Both) noexcept;

public:
    bool is_handshake_complete() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_handshake_done;
    }

    void set_verifier(std::shared_ptr<CertificateVerifier> v) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_verifier = std::move(v);
    }

    bool peer_certificate(Certificate& out) const noexcept;

    bool peer_chain(CertificateChain& out) const noexcept;

    std::string negotiated_cipher() const noexcept;

    TLSVersion negotiated_protocol_version() const noexcept;

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

    std::string to_string() const;

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
    bool run_handshake_unlocked() noexcept;

    bool renegotiate_unlocked() noexcept;

    int drain_plaintext(void* out, std::size_t len) noexcept;

    int send_raw(const std::vector<std::uint8_t>& data) noexcept;

    void send_close_notify_unlocked() noexcept;

    void close_unlocked() noexcept {
        send_close_notify_unlocked();
        m_socket.close();
        reset_state();
    }

    void reset_state() noexcept;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SSL_SOCKET_HPP