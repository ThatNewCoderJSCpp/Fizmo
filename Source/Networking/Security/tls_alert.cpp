#include "fizmo_library.hpp"
#include "tls_alert.hpp"

namespace fizmo {
namespace networking {
namespace security {

const char* alert_level_to_string(AlertLevel level) noexcept {
    return level == AlertLevel::Warning ? "warning" : "fatal";
}

const char* alert_description_to_string(AlertDescription d) noexcept {
    switch (d) {
        case AlertDescription::CloseNotify:                  return "close_notify";
        case AlertDescription::UnexpectedMessage:            return "unexpected_message";
        case AlertDescription::BadRecordMac:                 return "bad_record_mac";
        case AlertDescription::DecryptionFailed:             return "decryption_failed";
        case AlertDescription::RecordOverflow:               return "record_overflow";
        case AlertDescription::DecompressionFailure:         return "decompression_failure";
        case AlertDescription::HandshakeFailure:             return "handshake_failure";
        case AlertDescription::NoCertificate:                return "no_certificate";
        case AlertDescription::BadCertificate:               return "bad_certificate";
        case AlertDescription::UnsupportedCertificate:       return "unsupported_certificate";
        case AlertDescription::CertificateRevoked:           return "certificate_revoked";
        case AlertDescription::CertificateExpired:           return "certificate_expired";
        case AlertDescription::CertificateUnknown:           return "certificate_unknown";
        case AlertDescription::IllegalParameter:             return "illegal_parameter";
        case AlertDescription::UnknownCA:                    return "unknown_ca";
        case AlertDescription::AccessDenied:                 return "access_denied";
        case AlertDescription::DecodeError:                  return "decode_error";
        case AlertDescription::DecryptError:                 return "decrypt_error";
        case AlertDescription::ExportRestriction:            return "export_restriction";
        case AlertDescription::ProtocolVersion:              return "protocol_version";
        case AlertDescription::InsufficientSecurity:         return "insufficient_security";
        case AlertDescription::InternalError:                return "internal_error";
        case AlertDescription::InappropriateFallback:        return "inappropriate_fallback";
        case AlertDescription::UserCanceled:                 return "user_canceled";
        case AlertDescription::NoRenegotiation:              return "no_renegotiation";
        case AlertDescription::MissingExtension:             return "missing_extension";
        case AlertDescription::UnsupportedExtension:         return "unsupported_extension";
        case AlertDescription::CertificateUnobtainable:      return "certificate_unobtainable";
        case AlertDescription::UnrecognizedName:             return "unrecognized_name";
        case AlertDescription::BadCertificateStatusResponse: return "bad_certificate_status_response";
        case AlertDescription::BadCertificateHashValue:      return "bad_certificate_hash_value";
        case AlertDescription::UnknownPSKIdentity:           return "unknown_psk_identity";
        case AlertDescription::CertificateRequired:          return "certificate_required";
        case AlertDescription::NoApplicationProtocol:        return "no_application_protocol";
    }

    return "unknown_alert";
}

TLSAlert TLSAlert::from_info(const AlertInfo& info) noexcept {
    TLSAlert a;
    a.level = info.level == 1 ? AlertLevel::Warning : AlertLevel::Fatal;
    a.description = static_cast<AlertDescription>(info.description);
    a.received = info.received;
    return a;
}

bool TLSAlert::parse_record(const std::uint8_t* record, std::size_t len, TLSAlert& out) noexcept {
    if (!record || len < 7 || record[0] != 21) return false;
    const std::size_t body = (std::size_t(record[3]) << 8) | record[4];
    if (body != 2 || len < 5 + body) return false;
    out.level = record[5] == 1 ? AlertLevel::Warning : AlertLevel::Fatal;
    out.description = static_cast<AlertDescription>(record[6]);
    out.received = true;
    return true;
}

std::vector<std::uint8_t> TLSAlert::to_record(std::uint16_t version) const {
    return {
        21,
        static_cast<std::uint8_t>(version >> 8),
        static_cast<std::uint8_t>(version & 0xFF),
        0, 2,
        static_cast<std::uint8_t>(level),
        static_cast<std::uint8_t>(description)
    };
}

std::string TLSAlert::to_string() const {
    std::string s = "TLSAlert[";
    s += alert_level_to_string(level);
    s += ' ';
    s += alert_description_to_string(description);
    s += received ? " received]" : " sent]";
    return s;
}

} // namespace security
} // namespace networking
} // namespace fizmo
