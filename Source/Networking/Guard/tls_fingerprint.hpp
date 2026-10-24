#ifndef FIZMO_GUARD_TLS_FINGERPRINT_HPP
#define FIZMO_GUARD_TLS_FINGERPRINT_HPP

#include "guard_types.hpp"
#include "../Security/client_hello.hpp"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

enum class Transport : char {
    Tcp  = 't',
    Quic = 'q',
    Dtls = 'd'
};

struct TLSFingerprint {
    std::string ja4;
    std::string ja4_raw;
    std::string ja4_a;
    std::string ja4_b;
    std::string ja4_c;
    std::string tls_version;
    bool        has_sni = false;
    bool        uses_grease = false;
    std::string alpn;
    std::size_t cipher_count = 0;
    std::size_t extension_count = 0;

    bool empty() const noexcept { return ja4.empty(); }
};

bool is_grease(std::uint16_t value) noexcept;
TLSFingerprint fingerprint(const security::ClientHelloInfo& hello, Transport transport = Transport::Tcp);

enum class ClientFamily {
    Unknown,
    Chromium,
    Firefox,
    Safari,
    Automation,
    Missing
};

const char* to_string(ClientFamily family) noexcept;
ClientFamily classify_user_agent(const std::string& user_agent);

enum class RiskLevel {
    Low,
    Medium,
    High,
    Blocked
};

const char* to_string(RiskLevel level) noexcept;

struct FingerprintVerdict {
    int                      score = 0;
    RiskLevel                level = RiskLevel::Low;
    ClientFamily             claimed = ClientFamily::Unknown;
    std::string              label;
    std::vector<std::string> reasons;

    bool likely_bot() const noexcept { return level >= RiskLevel::High; }
    bool blocked() const noexcept { return level == RiskLevel::Blocked; }
};

struct FingerprintPolicy {
    int  medium_score = 30;
    int  high_score   = 60;
    bool require_tls  = false;
};

class FingerprintRegistry {
public:
    explicit FingerprintRegistry(FingerprintPolicy policy = FingerprintPolicy());

    void trust(const std::string& ja4, const std::string& label = {});
    void block(const std::string& ja4, const std::string& label = {});
    void forget(const std::string& ja4);
    bool trusted(const std::string& ja4) const;
    bool blocked(const std::string& ja4) const;

    FingerprintVerdict assess(const TLSFingerprint& fp, const std::string& user_agent) const;
    FingerprintVerdict assess_plaintext(const std::string& user_agent) const;

    const FingerprintPolicy& policy() const noexcept { return m_policy; }
    void set_policy(const FingerprintPolicy& policy);

private:
    FingerprintPolicy                            m_policy;
    mutable std::mutex                           m_mutex;
    std::unordered_map<std::string, std::string> m_trusted;
    std::unordered_map<std::string, std::string> m_blocked;

    void finish(FingerprintVerdict& v) const;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_TLS_FINGERPRINT_HPP
