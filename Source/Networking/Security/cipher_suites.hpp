#ifndef FIZMO_CIPHER_SUITES_HPP
#define FIZMO_CIPHER_SUITES_HPP

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace security {

enum class CipherSuite : std::uint16_t {
    TLS_AES_128_GCM_SHA256                        = 0x1301,
    TLS_AES_256_GCM_SHA384                        = 0x1302,
    TLS_CHACHA20_POLY1305_SHA256                  = 0x1303,
    TLS_AES_128_CCM_SHA256                        = 0x1304,
    TLS_AES_128_CCM_8_SHA256                      = 0x1305,
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256       = 0xC02B,
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384       = 0xC02C,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256         = 0xC02F,
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384         = 0xC030,
    TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256 = 0xCCA9,
    TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256   = 0xCCA8,
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256           = 0x009E,
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384           = 0x009F,
    TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256     = 0xCCAA,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256       = 0xC023,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384       = 0xC024,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256         = 0xC027,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384         = 0xC028,
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA          = 0xC009,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA          = 0xC00A,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA            = 0xC013,
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA            = 0xC014,
    TLS_RSA_WITH_AES_128_GCM_SHA256               = 0x009C,
    TLS_RSA_WITH_AES_256_GCM_SHA384               = 0x009D,
    TLS_RSA_WITH_AES_128_CBC_SHA                  = 0x002F,
    TLS_RSA_WITH_AES_256_CBC_SHA                  = 0x0035,
    TLS_RSA_WITH_3DES_EDE_CBC_SHA                 = 0x000A,
    TLS_RSA_WITH_RC4_128_SHA                      = 0x0005,
    TLS_RSA_WITH_RC4_128_MD5                      = 0x0004,
    TLS_RSA_WITH_DES_CBC_SHA                      = 0x0009,
    TLS_RSA_WITH_NULL_SHA256                      = 0x003B,
    TLS_RSA_WITH_NULL_SHA                         = 0x0002,
    TLS_RSA_WITH_NULL_MD5                         = 0x0001,
    TLS_RSA_EXPORT_WITH_RC4_40_MD5                = 0x0003
};

enum class CipherStrength : std::uint8_t {
    Weak = 0,
    Legacy,
    Secure,
    Modern
};

struct CipherSuiteInfo {
    CipherSuite    suite;
    const char*    iana_name;
    const char*    openssl_name;
    bool           tls13;
    bool           forward_secret;
    bool           aead;
    CipherStrength strength;
};

const CipherSuiteInfo* cipher_suite_info(CipherSuite suite) noexcept;
const CipherSuiteInfo* cipher_suite_info(const std::string& name) noexcept;
const std::vector<CipherSuiteInfo>& all_cipher_suites() noexcept;
const char* cipher_strength_to_string(CipherStrength s) noexcept;

class CipherList {
public:
    CipherList() = default;
    CipherList(std::initializer_list<CipherSuite> suites);

    static CipherList default_secure_list();
    static CipherList modern_list();
    static CipherList compatible_list();
    static CipherList tls13_only();
    static CipherList from_names(const std::vector<std::string>& names, std::vector<std::string>* unknown = nullptr);

    CipherList& add(CipherSuite suite);
    CipherList& remove(CipherSuite suite);
    CipherList& remove_weak();
    CipherList& remove_below(CipherStrength minimum);
    bool contains(CipherSuite suite) const noexcept;
    bool empty() const noexcept { return m_suites.empty(); }
    std::size_t size() const noexcept { return m_suites.size(); }
    const std::vector<CipherSuite>& suites() const noexcept { return m_suites; }
    bool has_weak() const noexcept;

    std::vector<std::string> iana_names() const;
    std::vector<std::string> openssl_names() const;
    std::vector<std::string> native_names() const;
    std::vector<unsigned>    to_schannel_algs() const;
    std::string              to_string() const;

private:
    std::vector<CipherSuite> m_suites;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CIPHER_SUITES_HPP
