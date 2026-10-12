#include "fizmo_library.hpp"
#include "ssl_error.hpp"

namespace fizmo {
namespace networking {
namespace security {

const char* ssl_error_code_to_string(SSLErrorCode code) noexcept {
    switch (code) {
        case SSLErrorCode::None:                       return "None";
        case SSLErrorCode::HandshakeFailed:            return "HandshakeFailed";
        case SSLErrorCode::CertificateExpired:         return "CertificateExpired";
        case SSLErrorCode::CertificateRevoked:         return "CertificateRevoked";
        case SSLErrorCode::CertificateUntrusted:       return "CertificateUntrusted";
        case SSLErrorCode::CertificateRequired:        return "CertificateRequired";
        case SSLErrorCode::CertificateInvalid:         return "CertificateInvalid";
        case SSLErrorCode::HostnameMismatch:           return "HostnameMismatch";
        case SSLErrorCode::PinViolation:               return "PinViolation";
        case SSLErrorCode::BadRecordMAC:               return "BadRecordMAC";
        case SSLErrorCode::DecryptionFailed:           return "DecryptionFailed";
        case SSLErrorCode::ProtocolVersionUnsupported: return "ProtocolVersionUnsupported";
        case SSLErrorCode::InsufficientSecurity:       return "InsufficientSecurity";
        case SSLErrorCode::NoSharedCipher:             return "NoSharedCipher";
        case SSLErrorCode::NoApplicationProtocol:      return "NoApplicationProtocol";
        case SSLErrorCode::AlertReceived:              return "AlertReceived";
        case SSLErrorCode::ContextNotInitialized:      return "ContextNotInitialized";
        case SSLErrorCode::InsecureConfiguration:      return "InsecureConfiguration";
        case SSLErrorCode::PeerClosed:                 return "PeerClosed";
        case SSLErrorCode::Timeout:                    return "Timeout";
        case SSLErrorCode::InternalError:              return "InternalError";
        case SSLErrorCode::IOError:                    return "IOError";
    }

    return "Unknown";
}

namespace {

class SSLCategory final : public std::error_category {
public:
    const char* name() const noexcept override { return "fizmo.ssl"; }
    std::string message(int code) const override { return ssl_error_code_to_string(static_cast<SSLErrorCode>(code)); }
};

} // namespace

const std::error_category& ssl_category() noexcept {
    static const SSLCategory category;
    return category;
}

SSLErrorCode ssl_error_from_alert(AlertDescription d) noexcept {
    switch (d) {
        case AlertDescription::CloseNotify:            return SSLErrorCode::PeerClosed;
        case AlertDescription::BadRecordMac:           return SSLErrorCode::BadRecordMAC;
        case AlertDescription::DecryptionFailed:
        case AlertDescription::DecryptError:           return SSLErrorCode::DecryptionFailed;
        case AlertDescription::CertificateRevoked:     return SSLErrorCode::CertificateRevoked;
        case AlertDescription::CertificateExpired:     return SSLErrorCode::CertificateExpired;
        case AlertDescription::UnknownCA:              return SSLErrorCode::CertificateUntrusted;
        case AlertDescription::BadCertificate:
        case AlertDescription::UnsupportedCertificate:
        case AlertDescription::CertificateUnknown:     return SSLErrorCode::CertificateInvalid;
        case AlertDescription::CertificateRequired:
        case AlertDescription::NoCertificate:          return SSLErrorCode::CertificateRequired;
        case AlertDescription::ProtocolVersion:        return SSLErrorCode::ProtocolVersionUnsupported;
        case AlertDescription::InsufficientSecurity:   return SSLErrorCode::InsufficientSecurity;
        case AlertDescription::HandshakeFailure:       return SSLErrorCode::HandshakeFailed;
        case AlertDescription::NoApplicationProtocol:  return SSLErrorCode::NoApplicationProtocol;
        case AlertDescription::InternalError:          return SSLErrorCode::InternalError;
        default:                                       return SSLErrorCode::AlertReceived;
    }
}

SSLErrorCode ssl_error_from_verify(VerifyError e) noexcept {
    switch (e) {
        case VerifyError::None:              return SSLErrorCode::None;
        case VerifyError::Expired:           return SSLErrorCode::CertificateExpired;
        case VerifyError::UntrustedRoot:     return SSLErrorCode::CertificateUntrusted;
        case VerifyError::HostnameMismatch:  return SSLErrorCode::HostnameMismatch;
        case VerifyError::Revoked:           return SSLErrorCode::CertificateRevoked;
        case VerifyError::SelfSigned:        return SSLErrorCode::CertificateUntrusted;
        case VerifyError::WeakSignature:     return SSLErrorCode::InsufficientSecurity;
        case VerifyError::RevocationUnknown: return SSLErrorCode::CertificateInvalid;
        case VerifyError::PinViolation:      return SSLErrorCode::PinViolation;
        case VerifyError::Rejected:          return SSLErrorCode::CertificateInvalid;
    }

    return SSLErrorCode::CertificateInvalid;
}

SSLErrorCode ssl_error_from_handshake(HandshakeError e) noexcept {
    switch (e) {
        case HandshakeError::None:                  return SSLErrorCode::None;
        case HandshakeError::CertificateRequired:   return SSLErrorCode::CertificateRequired;
        case HandshakeError::CertificateInvalid:    return SSLErrorCode::CertificateInvalid;
        case HandshakeError::HostnameMismatch:      return SSLErrorCode::HostnameMismatch;
        case HandshakeError::AlgorithmMismatch:     return SSLErrorCode::NoSharedCipher;
        case HandshakeError::Timeout:               return SSLErrorCode::Timeout;
        case HandshakeError::ContextNotInitialised: return SSLErrorCode::ContextNotInitialized;
        case HandshakeError::InternalError:         return SSLErrorCode::InternalError;
        default:                                    return SSLErrorCode::HandshakeFailed;
    }
}

SSLError SSLError::from_alert(const TLSAlert& alert, const std::string& detail) {
    SSLError e;
    e.code = ssl_error_from_alert(alert.description);
    e.has_alert = true;
    e.alert = alert;
    e.detail = detail.empty() ? alert.to_string() : detail;
    return e;
}

SSLError SSLError::from_verify(const VerifyResult& result) {
    SSLError e;
    e.code = ssl_error_from_verify(result.error);
    e.detail = result.detail;
    return e;
}

SSLError SSLError::from_handshake(HandshakeError error, const AlertInfo& alert, const std::string& detail) {
    SSLError e;
    e.code = ssl_error_from_handshake(error);
    e.detail = detail;

    if (alert.present) {
        e.has_alert = true;
        e.alert = TLSAlert::from_info(alert);

        if (e.code == SSLErrorCode::HandshakeFailed || e.code == SSLErrorCode::InternalError) {
            const SSLErrorCode by_alert = ssl_error_from_alert(e.alert.description);
            if (by_alert != SSLErrorCode::AlertReceived) e.code = by_alert;
        }
    }

    return e;
}

std::string SSLError::to_string() const {
    std::string s = "SSLError[";
    s += ssl_error_code_to_string(code);

    if (has_alert) {
        s += ' ';
        s += alert.to_string();
    }

    if (native_code != 0) {
        s += " native=";
        s += std::to_string(native_code);
    }

    if (!detail.empty()) {
        s += " (";
        s += detail;
        s += ')';
    }

    s += ']';
    return s;
}

} // namespace security
} // namespace networking
} // namespace fizmo
