#ifndef FIZMO_SSL_SOCKET_EVENT_HPP
#define FIZMO_SSL_SOCKET_EVENT_HPP

#include "ssl_error.hpp"
#include "tls_alert.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace fizmo {
namespace networking {
namespace security {

class SSLSocket;

enum class SSLEventType : std::uint8_t {
    HandshakeComplete,
    HandshakeFailed,
    Renegotiation,
    KeyUpdate,
    AlertReceived,
    AlertSent,
    SecureRead,
    SecureWrite,
    PeerClosed,
    Closed
};

const char* ssl_event_type_to_string(SSLEventType t) noexcept;

struct SSLEvent {
    SSLEventType type  = SSLEventType::Closed;
    std::size_t  bytes = 0;
    SSLError     error;
    TLSAlert     alert;

    std::string to_string() const;
};

class SSLEventHandler {
public:
    virtual ~SSLEventHandler() = default;

    virtual void on_handshake_complete(SSLSocket&) {}
    virtual void on_handshake_failed(SSLSocket&, const SSLError&) {}
    virtual void on_renegotiation(SSLSocket&) {}
    virtual void on_key_update(SSLSocket&) {}
    virtual void on_alert(SSLSocket&, const TLSAlert&) {}
    virtual void on_secure_read(SSLSocket&, std::size_t) {}
    virtual void on_secure_write(SSLSocket&, std::size_t) {}
    virtual void on_peer_closed(SSLSocket&) {}
    virtual void on_closed(SSLSocket&) {}
    virtual void on_event(SSLSocket&, const SSLEvent&) {}

    void dispatch(SSLSocket& socket, const SSLEvent& e);
};

class FunctionalSSLEventHandler final : public SSLEventHandler {
public:
    std::function<void(SSLSocket&)>                   handshake_complete;
    std::function<void(SSLSocket&, const SSLError&)>  handshake_failed;
    std::function<void(SSLSocket&)>                   renegotiation;
    std::function<void(SSLSocket&, const TLSAlert&)>  alert;
    std::function<void(SSLSocket&, std::size_t)>      secure_read;
    std::function<void(SSLSocket&, std::size_t)>      secure_write;
    std::function<void(SSLSocket&)>                   peer_closed;
    std::function<void(SSLSocket&)>                   closed;
    std::function<void(SSLSocket&, const SSLEvent&)>  any;

    void on_handshake_complete(SSLSocket& s) override { if (handshake_complete) handshake_complete(s); }
    void on_handshake_failed(SSLSocket& s, const SSLError& e) override { if (handshake_failed) handshake_failed(s, e); }
    void on_renegotiation(SSLSocket& s) override { if (renegotiation) renegotiation(s); }
    void on_alert(SSLSocket& s, const TLSAlert& a) override { if (alert) alert(s, a); }
    void on_secure_read(SSLSocket& s, std::size_t n) override { if (secure_read) secure_read(s, n); }
    void on_secure_write(SSLSocket& s, std::size_t n) override { if (secure_write) secure_write(s, n); }
    void on_peer_closed(SSLSocket& s) override { if (peer_closed) peer_closed(s); }
    void on_closed(SSLSocket& s) override { if (closed) closed(s); }
    void on_event(SSLSocket& s, const SSLEvent& e) override { if (any) any(s, e); }
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SSL_SOCKET_EVENT_HPP
