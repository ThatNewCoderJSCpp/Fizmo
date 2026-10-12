#ifndef FIZMO_TLS_ALERT_HPP
#define FIZMO_TLS_ALERT_HPP

#include "Backend/tls_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace security {

enum class AlertLevel : std::uint8_t {
    Warning = 1,
    Fatal   = 2
};

enum class AlertDescription : std::uint8_t {
    CloseNotify                   = 0,
    UnexpectedMessage             = 10,
    BadRecordMac                  = 20,
    DecryptionFailed              = 21,
    RecordOverflow                = 22,
    DecompressionFailure          = 30,
    HandshakeFailure              = 40,
    NoCertificate                 = 41,
    BadCertificate                = 42,
    UnsupportedCertificate        = 43,
    CertificateRevoked            = 44,
    CertificateExpired            = 45,
    CertificateUnknown            = 46,
    IllegalParameter              = 47,
    UnknownCA                     = 48,
    AccessDenied                  = 49,
    DecodeError                   = 50,
    DecryptError                  = 51,
    ExportRestriction             = 60,
    ProtocolVersion               = 70,
    InsufficientSecurity          = 71,
    InternalError                 = 80,
    InappropriateFallback         = 86,
    UserCanceled                  = 90,
    NoRenegotiation               = 100,
    MissingExtension              = 109,
    UnsupportedExtension          = 110,
    CertificateUnobtainable       = 111,
    UnrecognizedName              = 112,
    BadCertificateStatusResponse  = 113,
    BadCertificateHashValue       = 114,
    UnknownPSKIdentity            = 115,
    CertificateRequired           = 116,
    NoApplicationProtocol         = 120
};

const char* alert_level_to_string(AlertLevel level) noexcept;
const char* alert_description_to_string(AlertDescription description) noexcept;

struct TLSAlert {
    AlertLevel       level       = AlertLevel::Fatal;
    AlertDescription description = AlertDescription::InternalError;
    bool             received    = false;

    TLSAlert() noexcept = default;
    TLSAlert(AlertLevel l, AlertDescription d, bool from_peer = false) noexcept : level(l), description(d), received(from_peer) {}

    static TLSAlert from_info(const AlertInfo& info) noexcept;
    static bool parse_record(const std::uint8_t* record, std::size_t len, TLSAlert& out) noexcept;

    bool is_fatal() const noexcept { return level == AlertLevel::Fatal; }
    bool is_close_notify() const noexcept { return description == AlertDescription::CloseNotify; }

    std::vector<std::uint8_t> to_record(std::uint16_t version = 0x0303) const;
    std::string to_string() const;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_ALERT_HPP
