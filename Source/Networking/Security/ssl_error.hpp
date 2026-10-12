#ifndef FIZMO_SSL_ERROR_HPP
#define FIZMO_SSL_ERROR_HPP

#include "tls_alert.hpp"
#include "tls_handshake.hpp"

#include <string>
#include <system_error>

namespace fizmo {
namespace networking {
namespace security {

enum class SSLErrorCode {
    None = 0,
    HandshakeFailed,
    CertificateExpired,
    CertificateRevoked,
    CertificateUntrusted,
    CertificateRequired,
    CertificateInvalid,
    HostnameMismatch,
    PinViolation,
    BadRecordMAC,
    DecryptionFailed,
    ProtocolVersionUnsupported,
    InsufficientSecurity,
    NoSharedCipher,
    NoApplicationProtocol,
    AlertReceived,
    ContextNotInitialized,
    InsecureConfiguration,
    PeerClosed,
    Timeout,
    InternalError,
    IOError
};

const char* ssl_error_code_to_string(SSLErrorCode code) noexcept;

const std::error_category& ssl_category() noexcept;

inline std::error_code make_error_code(SSLErrorCode e) noexcept {
    return std::error_code(static_cast<int>(e), ssl_category());
}

struct SSLError {
    SSLErrorCode code = SSLErrorCode::None;
    bool         has_alert = false;
    TLSAlert     alert;
    int          native_code = 0;
    std::string  detail;

    explicit operator bool() const noexcept { return code != SSLErrorCode::None; }

    std::error_code error_code() const noexcept { return make_error_code(code); }
    std::string to_string() const;

    static SSLError from_alert(const TLSAlert& alert, const std::string& detail = {});
    static SSLError from_verify(const VerifyResult& result);
    static SSLError from_handshake(HandshakeError error, const AlertInfo& alert, const std::string& detail = {});
};

SSLErrorCode ssl_error_from_alert(AlertDescription d) noexcept;
SSLErrorCode ssl_error_from_verify(VerifyError e) noexcept;
SSLErrorCode ssl_error_from_handshake(HandshakeError e) noexcept;

} // namespace security
} // namespace networking
} // namespace fizmo

namespace std {
template <>
struct is_error_code_enum<fizmo::networking::security::SSLErrorCode> : true_type {};
} // namespace std

#endif // FIZMO_SSL_ERROR_HPP
