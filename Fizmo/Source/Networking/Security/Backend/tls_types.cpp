#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "tls_types.hpp"

namespace fizmo {
namespace networking {
namespace security {

auto KeyUsage::any() const noexcept -> bool {
        return digital_signature || key_encipherment || data_encipherment ||
               key_agreement     || cert_sign        || crl_sign          ||
               non_repudiation   || encipher_only    || decipher_only;
    }

auto CertificateInfo::clear() noexcept -> void {
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
        default:                              return "Unknown";
    }
}

auto VerifyResult::to_string() const -> std::string {
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

} // namespace security
} // namespace networking
} // namespace fizmo
