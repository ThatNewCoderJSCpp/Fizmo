#ifndef FIZMO_SSL_SOCKET_HPP
#define FIZMO_SSL_SOCKET_HPP

#include "tls_context.hpp"
#include "tls_handshake.hpp"
#include "certificate_verifier.hpp"
#include "certificate.hpp"
#include "../Core/tcp_socket.hpp"
#include "../Core/buffer.hpp"
#include "../Core/socket_error.hpp"
#include "../../Basic/basic_includes.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <memory>
#include <functional>

namespace fizmo {
namespace networking {
namespace security {

class SSLSocket {
public:
    explicit SSLSocket(TLSContext& ctx, core::AddressFamily family = core::AddressFamily::IPv4) noexcept
        : m_ctx(ctx),
          m_socket(family),
          m_handshake_done(false),
          m_shutdown_sent(false)
    #ifdef OS_WINDOWS
        , m_sizes_queried(false)
    #endif
    {}

    ~SSLSocket() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        close_unlocked();
    }

    SSLSocket(const SSLSocket&)            = delete;
    SSLSocket& operator=(const SSLSocket&) = delete;

    bool connect(const core::NetworkAddress& addr, const std::string& hostname = {}) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_socket.connect(addr)) return false;
        m_hostname = hostname.empty() ? addr.host() : hostname;
        m_handshake = std::make_unique<TLSHandshake>(m_ctx, HandshakeRole::Client, m_hostname);
        m_handshake->set_verifier(m_verifier);

        return run_handshake_unlocked();
    }

    bool accept(std::unique_ptr<core::TCPSocket> raw_conn) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!raw_conn) return false;
        m_socket = std::move(*raw_conn);
        m_handshake = std::make_unique<TLSHandshake>(m_ctx, HandshakeRole::Server);
        m_handshake->set_verifier(m_verifier);
        return run_handshake_unlocked();
    }

    int send(const void* data, std::size_t len) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !data || len == 0) return -1;

    #ifdef OS_WINDOWS
        if (!ensure_stream_sizes()) return -1;
        const auto* src = static_cast<const std::uint8_t*>(data);
        std::size_t total_sent = 0;

        while (total_sent < len) {
            DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(len - total_sent, m_sizes.cbMaximumMessage));
            DWORD buf_len = m_sizes.cbHeader + chunk + m_sizes.cbTrailer;
            std::vector<std::uint8_t> msg(buf_len);
            std::memcpy(msg.data() + m_sizes.cbHeader, src + total_sent, chunk);
            SecBuffer bufs[4] = {};
            bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
            bufs[0].cbBuffer   = m_sizes.cbHeader;
            bufs[0].pvBuffer   = msg.data();
            bufs[1].BufferType = SECBUFFER_DATA;
            bufs[1].cbBuffer   = chunk;
            bufs[1].pvBuffer   = msg.data() + m_sizes.cbHeader;
            bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
            bufs[2].cbBuffer   = m_sizes.cbTrailer;
            bufs[2].pvBuffer   = msg.data() + m_sizes.cbHeader + chunk;
            bufs[3].BufferType = SECBUFFER_EMPTY;
            SecBufferDesc desc = {};
            desc.ulVersion = SECBUFFER_VERSION;
            desc.cBuffers  = 4;
            desc.pBuffers  = bufs;

            SECURITY_STATUS ss = EncryptMessage(
                const_cast<PCtxtHandle>(&m_handshake->security_context()),
                0, 
                &desc, 
                0
            );

            if (FAILED(ss)) return total_sent > 0 ? static_cast<int>(total_sent) : -1;
            DWORD wire_len = bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer;
            int n = m_socket.send(msg.data(), wire_len);
            if (n <= 0) return total_sent > 0 ? static_cast<int>(total_sent) : n;
            total_sent += chunk;
        }

        return static_cast<int>(total_sent);
    #else
        (void)data; (void)len;
        return -1;
    #endif
    }

    int send_string(const std::string& str) noexcept { return send(str.data(), str.size()); }

    int receive(void* buffer, std::size_t len) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_handshake_done || !buffer || len == 0) return -1;
        if (!m_plaintext_buf.empty()) { return drain_plaintext(buffer, len); }

    #ifdef OS_WINDOWS
        return decrypt_from_wire(buffer, len);
    #else
        return -1;
    #endif
    }

    bool receive_string(std::string& out, std::size_t max_len = 4096) noexcept {
        out.resize(max_len);
        int n = receive(&out[0], max_len);
        if (n <= 0) { out.clear(); return false; }
        out.resize(static_cast<std::size_t>(n));
        return true;
    }

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

    #ifdef OS_WINDOWS
        if (!m_handshake_done || !m_handshake) return false;
        PCCERT_CONTEXT peer = nullptr;

        SECURITY_STATUS qs = QueryContextAttributes(
            const_cast<PCtxtHandle>(&m_handshake->security_context()),
            SECPKG_ATTR_REMOTE_CERT_CONTEXT, 
            &peer
        );

        if (FAILED(qs) || !peer) return false;
        bool ok = out.load_from_der(peer->pbCertEncoded, peer->cbCertEncoded);
        CertFreeCertificateContext(peer);
        return ok;
    #else
        (void)out;
        return false;
    #endif
    }

    std::string negotiated_cipher() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);

    #ifdef OS_WINDOWS
        if (!m_handshake_done || !m_handshake) return {};
        SecPkgContext_CipherInfo info = {};
        info.dwVersion = SECPKGCONTEXT_CIPHERINFO_V1;

        SECURITY_STATUS qs = QueryContextAttributes(
            const_cast<PCtxtHandle>(&m_handshake->security_context()),
            SECPKG_ATTR_CIPHER_INFO, 
            &info
        );

        if (FAILED(qs)) return {};
        std::string name;
        for (int i = 0; i < 64 && info.szCipherSuite[i]; ++i) { name += static_cast<char>(info.szCipherSuite[i]); }
        return name;
    #else
        return {};
    #endif
    }

    TLSVersion negotiated_protocol_version() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);

    #ifdef OS_WINDOWS
        if (!m_handshake_done || !m_handshake) return TLSVersion::TLS_1_2;
        SecPkgContext_ConnectionInfo conn = {};

        SECURITY_STATUS qs = QueryContextAttributes(
            const_cast<PCtxtHandle>(&m_handshake->security_context()),
            SECPKG_ATTR_CONNECTION_INFO,
            &conn
        );

        if (FAILED(qs)) return TLSVersion::TLS_1_2;

        switch (conn.dwProtocol) {
            case SP_PROT_TLS1_0_CLIENT: case SP_PROT_TLS1_0_SERVER:
                return TLSVersion::TLS_1_0;
            case SP_PROT_TLS1_1_CLIENT: case SP_PROT_TLS1_1_SERVER:
                return TLSVersion::TLS_1_1;
            case SP_PROT_TLS1_2_CLIENT: case SP_PROT_TLS1_2_SERVER:
                return TLSVersion::TLS_1_2;
        #ifdef SP_PROT_TLS1_3
            case SP_PROT_TLS1_3_CLIENT: case SP_PROT_TLS1_3_SERVER:
                return TLSVersion::TLS_1_3;
        #endif
            default: return TLSVersion::TLS_1_2;
        }
    #else
        return TLSVersion::TLS_1_2;
    #endif
    }

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

        if (m_handshake_done) {
            s += " proto=";
            s += TLSContext::version_to_string(const_cast<SSLSocket*>(this)->negotiated_protocol_version());
        }

        s += ']';
        return s;
    }

private:
    mutable std::mutex                    m_mutex;
    TLSContext&                           m_ctx;
    core::TCPSocket                       m_socket;
    std::string                           m_hostname;
    std::unique_ptr<TLSHandshake>         m_handshake;
    std::shared_ptr<CertificateVerifier>  m_verifier;
    bool                                  m_handshake_done;
    bool                                  m_shutdown_sent;

    std::vector<std::uint8_t>             m_plaintext_buf;
    std::vector<std::uint8_t>             m_cipher_buf;

#ifdef OS_WINDOWS
    SecPkgContext_StreamSizes             m_sizes;
    bool                                  m_sizes_queried;
#endif

    bool run_handshake_unlocked() noexcept {
        HandshakeResult hr = m_handshake->start();
        if (hr.has_failed()) return false;
        if (hr.has_output()) { if (send_raw(hr.output) <= 0) return false; }
        std::vector<std::uint8_t> buf(16384);

        while (!hr.is_complete()) {
            if (hr.has_failed()) return false;
            int n = m_socket.receive(buf.data(), buf.size());
            if (n <= 0) return false;
            hr = m_handshake->step(buf.data(), static_cast<std::size_t>(n));
            if (hr.has_output()) { if (send_raw(hr.output) <= 0) return false; }

            if (hr.is_complete() && !hr.extra.empty()) {
                m_cipher_buf.insert(m_cipher_buf.end(), hr.extra.begin(), hr.extra.end());
            }

            if (hr.state == HandshakeState::Renegotiating) { hr.state = HandshakeState::InProgress; }
        }

        m_handshake_done = true;
        return true;
    }

#ifdef OS_WINDOWS
    bool ensure_stream_sizes() noexcept {
        if (m_sizes_queried) return true;
        std::memset(&m_sizes, 0, sizeof(m_sizes));

        SECURITY_STATUS ss = QueryContextAttributes(
            const_cast<PCtxtHandle>(&m_handshake->security_context()),
            SECPKG_ATTR_STREAM_SIZES, 
            &m_sizes
        );

        m_sizes_queried = SUCCEEDED(ss);
        return m_sizes_queried;
    }

    int decrypt_from_wire(void* out, std::size_t out_len) noexcept {
        if (!ensure_stream_sizes()) return -1;

        for (;;) {
            if (!m_cipher_buf.empty()) {
                int result = try_decrypt(out, out_len);
                if (result != 0) return result;  
            }

            std::size_t want = m_cipher_buf.empty() ? 8192 : std::max<std::size_t>(4096, m_sizes.cbHeader + m_sizes.cbMaximumMessage + m_sizes.cbTrailer - m_cipher_buf.size());
            std::size_t old_sz = m_cipher_buf.size();
            m_cipher_buf.resize(old_sz + want);
            int n = m_socket.receive(m_cipher_buf.data() + old_sz, want);

            if (n <= 0) {
                m_cipher_buf.resize(old_sz);
                return n; 
            }

            m_cipher_buf.resize(old_sz + static_cast<std::size_t>(n));
        }
    }

    int try_decrypt(void* out, std::size_t out_len) noexcept {
        SecBuffer bufs[4] = {};
        bufs[0].BufferType = SECBUFFER_DATA;
        bufs[0].cbBuffer   = static_cast<ULONG>(m_cipher_buf.size());
        bufs[0].pvBuffer   = m_cipher_buf.data();
        bufs[1].BufferType = SECBUFFER_EMPTY;
        bufs[2].BufferType = SECBUFFER_EMPTY;
        bufs[3].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc desc = {};
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers  = 4;
        desc.pBuffers  = bufs;

        SECURITY_STATUS ss = DecryptMessage(
            const_cast<PCtxtHandle>(&m_handshake->security_context()),
            &desc, 
            0, 
            nullptr
        );

        if (ss == SEC_E_INCOMPLETE_MESSAGE) { return 0; }

        if (ss == SEC_I_CONTEXT_EXPIRED) {
            consume_cipher_buf(bufs);
            return 0;
        }

        if (ss == SEC_I_RENEGOTIATE) {
            consume_cipher_buf(bufs);
            handle_renegotiation();
            return 0;  
        }

        if (FAILED(ss)) { return -1; }
        int copied = 0;

        for (int i = 0; i < 4; ++i) {
            if (bufs[i].BufferType == SECBUFFER_DATA && bufs[i].cbBuffer > 0 && bufs[i].pvBuffer) {
                auto* p   = static_cast<std::uint8_t*>(bufs[i].pvBuffer);
                DWORD amt = bufs[i].cbBuffer;
                std::size_t to_copy = std::min<std::size_t>(amt, out_len);
                std::memcpy(out, p, to_copy);
                copied = static_cast<int>(to_copy);
                if (to_copy < amt) { m_plaintext_buf.insert(m_plaintext_buf.end(), p + to_copy, p + amt); }
                break;
            }
        }

        consume_cipher_buf(bufs);
        return copied;
    }

    void consume_cipher_buf(SecBuffer bufs[4]) noexcept {
        for (int i = 0; i < 4; ++i) {
            if (bufs[i].BufferType == SECBUFFER_EXTRA && bufs[i].cbBuffer > 0) {
                std::size_t extra = bufs[i].cbBuffer;
                std::size_t off   = m_cipher_buf.size() - extra;
                std::memmove(m_cipher_buf.data(), m_cipher_buf.data() + off, extra);
                m_cipher_buf.resize(extra);
                return;
            }
        }

        m_cipher_buf.clear();
    }

    void handle_renegotiation() noexcept {
        if (!m_handshake) return;
        HandshakeResult hr = m_handshake->step(m_cipher_buf.data(), m_cipher_buf.size());
        m_cipher_buf.clear();

        while (!hr.is_complete() && !hr.has_failed()) {
            if (hr.has_output()) {
                if (send_raw(hr.output) <= 0) {
                    m_handshake_done = false;
                    return;
                }
            }

            std::vector<std::uint8_t> buf(8192);
            int n = m_socket.receive(buf.data(), buf.size());
            if (n <= 0) { m_handshake_done = false; return; }
            hr = m_handshake->step(buf.data(), static_cast<std::size_t>(n));
        }

        if (hr.has_failed()) { m_handshake_done = false; }
    }

    void send_close_notify_unlocked() noexcept {
        if (!m_handshake_done || m_shutdown_sent) return;
        m_shutdown_sent = true;
        DWORD shutdown_token = SCHANNEL_SHUTDOWN;
        SecBuffer token_buf  = {};
        token_buf.BufferType = SECBUFFER_TOKEN;
        token_buf.cbBuffer   = sizeof(shutdown_token);
        token_buf.pvBuffer   = &shutdown_token;
        SecBufferDesc token_desc = {};
        token_desc.ulVersion = SECBUFFER_VERSION;
        token_desc.cBuffers  = 1;
        token_desc.pBuffers  = &token_buf;
        ApplyControlToken(const_cast<PCtxtHandle>(&m_handshake->security_context()), &token_desc);
        SecBuffer out_buf     = {};
        out_buf.BufferType    = SECBUFFER_TOKEN;
        SecBufferDesc out_desc = {};
        out_desc.ulVersion     = SECBUFFER_VERSION;
        out_desc.cBuffers      = 1;
        out_desc.pBuffers      = &out_buf;
        DWORD       flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT
                          | ISC_REQ_CONFIDENTIALITY | ISC_REQ_STREAM
                          | ISC_REQ_ALLOCATE_MEMORY;

        DWORD       out_flags = 0;
        TimeStamp   expiry    = {};
        CredHandle  cred      = m_ctx.credential_handle();

        InitializeSecurityContextW(
            &cred,
            const_cast<PCtxtHandle>(&m_handshake->security_context()),
            nullptr, 
            flags, 
            0, 
            0,
            nullptr, 
            0, 
            nullptr,
            &out_desc, 
            &out_flags, 
            &expiry
        );

        if (out_buf.cbBuffer > 0 && out_buf.pvBuffer) {
            m_socket.send(out_buf.pvBuffer, static_cast<std::size_t>(out_buf.cbBuffer));
            FreeContextBuffer(out_buf.pvBuffer);
        }
    }

#endif // OS_WINDOWS

    int drain_plaintext(void* out, std::size_t len) noexcept {
        std::size_t avail = std::min(len, m_plaintext_buf.size());
        std::memcpy(out, m_plaintext_buf.data(), avail);

        if (avail == m_plaintext_buf.size()) {
            m_plaintext_buf.clear();
        } else {
            m_plaintext_buf.erase(m_plaintext_buf.begin(), m_plaintext_buf.begin() + static_cast<std::ptrdiff_t>(avail));
        }

        return static_cast<int>(avail);
    }

    int send_raw(const std::vector<std::uint8_t>& data) noexcept {
        std::size_t sent = 0;

        while (sent < data.size()) {
            int n = m_socket.send(data.data() + sent, data.size() - sent);
            if (n <= 0) return n;
            sent += static_cast<std::size_t>(n);
        }

        return static_cast<int>(sent);
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
        m_plaintext_buf.clear();
        m_cipher_buf.clear();
    #ifdef OS_WINDOWS
        m_sizes_queried = false;
        std::memset(&m_sizes, 0, sizeof(m_sizes));
    #endif
    }

#ifndef OS_WINDOWS
    void send_close_notify_unlocked() noexcept {}
#endif
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SSL_SOCKET_HPP