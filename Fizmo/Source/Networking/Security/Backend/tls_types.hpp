#ifndef FIZMO_TLS_TYPES_HPP
#define FIZMO_TLS_TYPES_HPP

#include "../../../Basic/basic_includes.hpp"
#include <string>
#include <vector>
#include <cstdint>

#ifdef OS_LINUX
#include "../../../x11_compat.hpp"
#endif

namespace fizmo {
namespace networking {
namespace security {

enum class TLSVersion : std::uint32_t {
    TLS_1_0 = 0x0301,
    TLS_1_1 = 0x0302,
    TLS_1_2 = 0x0303,
    TLS_1_3 = 0x0304
};

enum class VerifyMode {
    None = 0,       
    Peer,          
    FailIfNoPeer   
};

enum class ContextUse {
    Client = 0,
    Server
};

struct CertificateData {
    std::vector<std::uint8_t> chain;        
    std::vector<std::uint8_t> private_key;  
    std::string               password;     

    bool empty() const noexcept { return chain.empty(); }
    void clear() noexcept { chain.clear(); private_key.clear(); password.clear(); }
};

struct TLSConfig {
    TLSVersion               min_version = TLSVersion::TLS_1_2;
    TLSVersion               max_version = TLSVersion::TLS_1_3;
    std::vector<std::string> cipher_suites;
    CertificateData          certificate;
    std::string              ca_store_path;
    VerifyMode               verify_mode = VerifyMode::Peer;
    ContextUse               use         = ContextUse::Client;
    bool                     check_revocation = true;
};

struct KeyUsage {
    bool digital_signature = false;
    bool key_encipherment  = false;
    bool data_encipherment = false;
    bool key_agreement     = false;
    bool cert_sign         = false;
    bool crl_sign          = false;
    bool non_repudiation   = false;
    bool encipher_only     = false;
    bool decipher_only     = false;

    bool any() const noexcept {
        return digital_signature || key_encipherment || data_encipherment ||
               key_agreement     || cert_sign        || crl_sign          ||
               non_repudiation   || encipher_only    || decipher_only;
    }
};

struct SubjectAltName {
    enum class Type { DNS, IP };
    Type        type;
    std::string value;
};

struct CertificateInfo {
    std::string                 subject;
    std::string                 issuer;
    std::string                 serial;
    std::int64_t                not_before = 0;   
    std::int64_t                not_after  = 0;   
    std::vector<SubjectAltName> sans;
    KeyUsage                    key_usage;

    void clear() noexcept {
        subject.clear();
        issuer.clear();
        serial.clear();
        not_before = 0;
        not_after  = 0;
        sans.clear();
        key_usage = KeyUsage{};
    }
};

enum class VerifyError : std::uint8_t {
    None = 0,
    Expired,
    UntrustedRoot,
    HostnameMismatch,
    Revoked,
    SelfSigned,
    WeakSignature,
    RevocationUnknown   
};

inline const char* verify_error_to_string(VerifyError e) noexcept {
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

struct VerifyResult {
    bool        passed = false;
    VerifyError error  = VerifyError::None;
    int         depth  = -1;
    std::string detail;

    explicit operator bool() const noexcept { return passed; }

    static VerifyResult success() noexcept { return { true, VerifyError::None, -1, {} }; }

    static VerifyResult failure(VerifyError err, int chain_depth = 0, const std::string& msg = {}) noexcept {
        return { false, err, chain_depth, msg };
    }

    std::string to_string() const {
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
};

inline const char* tls_version_to_string(TLSVersion v) noexcept {
    switch (v) {
        case TLSVersion::TLS_1_0: return "TLS 1.0";
        case TLSVersion::TLS_1_1: return "TLS 1.1";
        case TLSVersion::TLS_1_2: return "TLS 1.2";
        case TLSVersion::TLS_1_3: return "TLS 1.3";
        default:                  return "Unknown";
    }
}

inline const char* verify_mode_to_string(VerifyMode m) noexcept {
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

#endif // FIZMO_TLS_TYPES_HPP