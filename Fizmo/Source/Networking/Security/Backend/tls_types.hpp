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

    bool any() const noexcept;
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

    void clear() noexcept;
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

 const char* verify_error_to_string(VerifyError e) noexcept;

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

    std::string to_string() const;
};

 const char* tls_version_to_string(TLSVersion v) noexcept;

 const char* verify_mode_to_string(VerifyMode m) noexcept;

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_TYPES_HPP