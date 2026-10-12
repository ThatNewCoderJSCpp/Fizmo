#include "fizmo_library.hpp"
#include "tls_types.hpp"

namespace fizmo {
namespace networking {
namespace security {

bool KeyUsage::any() const noexcept {
    return digital_signature || key_encipherment || data_encipherment ||
           key_agreement     || cert_sign        || crl_sign          ||
           non_repudiation   || encipher_only    || decipher_only;
}

void CertificateInfo::clear() noexcept {
    subject.clear();
    issuer.clear();
    serial.clear();
    not_before = 0;
    not_after  = 0;
    sans.clear();
    key_usage = KeyUsage{};
}

const char* verify_error_to_string(VerifyError e) noexcept {
    switch (e) {
        case VerifyError::None:               return "None";
        case VerifyError::Expired:            return "Expired";
        case VerifyError::UntrustedRoot:      return "UntrustedRoot";
        case VerifyError::HostnameMismatch:   return "HostnameMismatch";
        case VerifyError::Revoked:            return "Revoked";
        case VerifyError::SelfSigned:         return "SelfSigned";
        case VerifyError::WeakSignature:      return "WeakSignature";
        case VerifyError::RevocationUnknown:  return "RevocationUnknown";
        case VerifyError::PinViolation:       return "PinViolation";
        case VerifyError::Rejected:           return "Rejected";
        default:                              return "Unknown";
    }
}

std::string VerifyResult::to_string() const {
    std::string s = passed ? "PASS" : "FAIL";

    if (!passed) {
        s += " error=";
        s += verify_error_to_string(error);

        if (depth >= 0) {
            s += " depth=";
            s += std::to_string(depth);
        }

        if (!detail.empty()) {
            s += " (";
            s += detail;
            s += ')';
        }
    }

    return s;
}

const char* tls_version_to_string(TLSVersion v) noexcept {
    switch (v) {
        case TLSVersion::TLS_1_0: return "TLS 1.0";
        case TLSVersion::TLS_1_1: return "TLS 1.1";
        case TLSVersion::TLS_1_2: return "TLS 1.2";
        case TLSVersion::TLS_1_3: return "TLS 1.3";
        default:                  return "Unknown";
    }
}

const char* verify_mode_to_string(VerifyMode m) noexcept {
    switch (m) {
        case VerifyMode::None:         return "None";
        case VerifyMode::Peer:         return "Peer";
        case VerifyMode::FailIfNoPeer: return "FailIfNoPeer";
        default:                       return "Unknown";
    }
}

const char* ocsp_status_to_string(OCSPCertStatus s) noexcept {
    switch (s) {
        case OCSPCertStatus::Unknown:     return "Unknown";
        case OCSPCertStatus::Good:        return "Good";
        case OCSPCertStatus::Revoked:     return "Revoked";
        case OCSPCertStatus::Invalid:     return "Invalid";
        case OCSPCertStatus::Unsupported: return "Unsupported";
        default:                          return "Unknown";
    }
}

const char* key_type_to_string(KeyType t) noexcept {
    switch (t) {
        case KeyType::None:    return "None";
        case KeyType::RSA:     return "RSA";
        case KeyType::EC:      return "EC";
        case KeyType::Ed25519: return "Ed25519";
        case KeyType::Ed448:   return "Ed448";
        case KeyType::DSA:     return "DSA";
        default:               return "Other";
    }
}

} // namespace security
} // namespace networking
} // namespace fizmo
