#include "fizmo_library.hpp"
#include "ssl_socket_event.hpp"

namespace fizmo {
namespace networking {
namespace security {

const char* ssl_event_type_to_string(SSLEventType t) noexcept {
    switch (t) {
        case SSLEventType::HandshakeComplete: return "HandshakeComplete";
        case SSLEventType::HandshakeFailed:   return "HandshakeFailed";
        case SSLEventType::Renegotiation:     return "Renegotiation";
        case SSLEventType::KeyUpdate:         return "KeyUpdate";
        case SSLEventType::AlertReceived:     return "AlertReceived";
        case SSLEventType::AlertSent:         return "AlertSent";
        case SSLEventType::SecureRead:        return "SecureRead";
        case SSLEventType::SecureWrite:       return "SecureWrite";
        case SSLEventType::PeerClosed:        return "PeerClosed";
        case SSLEventType::Closed:            return "Closed";
    }

    return "Unknown";
}

std::string SSLEvent::to_string() const {
    std::string s = "SSLEvent[";
    s += ssl_event_type_to_string(type);
    if (bytes) s += " bytes=" + std::to_string(bytes);
    if (error) s += " " + error.to_string();
    if (type == SSLEventType::AlertReceived || type == SSLEventType::AlertSent) s += " " + alert.to_string();
    s += ']';
    return s;
}

void SSLEventHandler::dispatch(SSLSocket& socket, const SSLEvent& e) {
    switch (e.type) {
        case SSLEventType::HandshakeComplete: on_handshake_complete(socket); break;
        case SSLEventType::HandshakeFailed:   on_handshake_failed(socket, e.error); break;
        case SSLEventType::Renegotiation:     on_renegotiation(socket); break;
        case SSLEventType::KeyUpdate:         on_key_update(socket); break;
        case SSLEventType::AlertReceived:
        case SSLEventType::AlertSent:         on_alert(socket, e.alert); break;
        case SSLEventType::SecureRead:        on_secure_read(socket, e.bytes); break;
        case SSLEventType::SecureWrite:       on_secure_write(socket, e.bytes); break;
        case SSLEventType::PeerClosed:        on_peer_closed(socket); break;
        case SSLEventType::Closed:            on_closed(socket); break;
    }

    on_event(socket, e);
}

} // namespace security
} // namespace networking
} // namespace fizmo
