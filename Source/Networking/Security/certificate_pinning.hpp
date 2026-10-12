#ifndef FIZMO_CERTIFICATE_PINNING_HPP
#define FIZMO_CERTIFICATE_PINNING_HPP

#include "certificate_verifier.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace security {

using PinDigest = std::array<std::uint8_t, 32>;

struct PinViolation {
    std::string              host;
    std::vector<std::string> presented;
    std::vector<std::string> expected;
};

class PinSet {
public:
    PinSet& add_pin(const std::string& host, const std::string& sha256_base64, bool include_subdomains = false);
    PinSet& add_pin(const std::string& host, const PinDigest& digest, bool include_subdomains = false);
    PinSet& add_certificate(const std::string& host, const Certificate& cert, bool include_subdomains = false);
    PinSet& set_expiry(const std::string& host, std::chrono::system_clock::time_point expires);
    PinSet& remove_host(const std::string& host);
    void clear();

    bool has_pins(const std::string& host) const;
    bool check(const std::string& host, const CertificateChain& chain, PinViolation* violation = nullptr) const;
    bool check(const std::string& host, const Certificate& leaf, PinViolation* violation = nullptr) const;
    std::size_t size() const;

    static bool spki_sha256(const Certificate& cert, PinDigest& out);
    static bool spki_sha256(const std::vector<std::uint8_t>& cert_der, PinDigest& out);
    static std::string spki_sha256_base64(const Certificate& cert);

private:
    struct Rule {
        std::string                           pattern;
        bool                                  include_subdomains = false;
        std::vector<PinDigest>                pins;
        std::chrono::system_clock::time_point expires = std::chrono::system_clock::time_point::max();
    };

    mutable std::mutex m_mutex;
    std::vector<Rule>  m_rules;

    Rule& rule_for(const std::string& host, bool include_subdomains);
    const Rule* match(const std::string& host) const;
    static bool host_matches(const Rule& rule, const std::string& host);
    bool check_digests(const std::string& host, const std::vector<PinDigest>& presented, PinViolation* violation) const;
};

class PinningVerifier : public CertificateVerifier {
public:
    PinningVerifier() = default;
    explicit PinningVerifier(std::shared_ptr<PinSet> pins) : m_pins(std::move(pins)) {}

    void set_pins(std::shared_ptr<PinSet> pins) { m_pins = std::move(pins); }
    std::shared_ptr<PinSet> pins() const { return m_pins; }

    void set_violation_callback(std::function<void(const PinViolation&)> cb) { m_on_violation = std::move(cb); }
    void set_report_only(bool enable) noexcept { m_report_only = enable; }

    using CertificateVerifier::verify;

    VerifyResult verify(const std::string& hostname, const Certificate& leaf, const CertificateChain* chain) const noexcept override;

private:
    std::shared_ptr<PinSet>                    m_pins;
    std::function<void(const PinViolation&)>   m_on_violation;
    bool                                       m_report_only = false;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CERTIFICATE_PINNING_HPP
