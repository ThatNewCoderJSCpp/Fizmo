#ifndef FIZMO_ASYNC_TLS_STREAM_HPP
#define FIZMO_ASYNC_TLS_STREAM_HPP

#include "tcp.hpp"
#include "../Security/tls_context.hpp"
#include "../Security/tls_handshake.hpp"
#include "../Security/ssl_error.hpp"
#include "../Security/ssl_socket.hpp"
#include "../Security/certificate.hpp"
#include "../Security/certificate_chain.hpp"

#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace async {

enum class HandshakeType {
    Client,
    Server
};

class TlsStream final : public AsyncStream {
public:
    TlsStream(IOContext& ctx, security::TLSContext& tls);
    TlsStream(TcpStream&& socket, security::TLSContext& tls);
    ~TlsStream() override;

    TlsStream(const TlsStream&) = delete;
    TlsStream& operator=(const TlsStream&) = delete;
    TlsStream(TlsStream&& other) noexcept = default;
    TlsStream& operator=(TlsStream&& other) noexcept = default;

public:
    IOContext& context() const noexcept override;
    TcpStream& next_layer() noexcept;
    const TcpStream& next_layer() const noexcept;
    security::TLSContext& tls_context() const noexcept;

    void set_hostname(const std::string& hostname);
    void set_session_key(const std::string& key);
    void set_verifier(std::shared_ptr<security::CertificateVerifier> verifier);
    void set_handshake_timeout(std::chrono::milliseconds timeout);

    void async_handshake(HandshakeType type, ErrorHandler handler);
    void async_connect(const Endpoint& endpoint, const std::string& hostname, ErrorHandler handler);

    void async_read_some(MutableBuffer buffer, IoHandler handler) override;
    void async_write_some(ConstBuffer buffer, IoHandler handler) override;
    void async_shutdown(ErrorHandler handler) override;
    void async_shutdown(security::TLSShutdownMode mode, ErrorHandler handler);
    void async_key_update(bool request_peer, ErrorHandler handler);
    void async_request_client_certificate(ErrorHandler handler);

    void close() override;
    void cancel() override;
    bool is_open() const noexcept override;
    Endpoint local_endpoint() const override;
    Endpoint remote_endpoint() const override;
    void set_timeout(std::chrono::milliseconds timeout) override;
    void clear_timeout() override;
    bool is_secure() const noexcept override { return true; }
    std::string description() const override;

public:
    bool handshake_complete() const noexcept;
    bool peer_closed() const noexcept;
    std::string negotiated_alpn() const;
    std::string negotiated_cipher() const;
    security::TLSVersion negotiated_protocol_version() const;
    std::string requested_server_name() const;
    bool session_resumed() const;
    bool peer_certificate(security::Certificate& out) const;
    bool peer_chain(security::CertificateChain& out) const;
    bool export_keying_material(const std::string& label, const std::vector<std::uint8_t>& context, std::size_t length, std::vector<std::uint8_t>& out, bool use_context = true) const;
    bool channel_binding(security::ChannelBindingType type, std::vector<std::uint8_t>& out) const;
    std::vector<std::uint8_t> ocsp_response() const;
    security::SSLError last_error() const;
    security::TLSMetrics metrics() const;

    struct Impl;

private:
    std::shared_ptr<Impl> m_impl;
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_TLS_STREAM_HPP
