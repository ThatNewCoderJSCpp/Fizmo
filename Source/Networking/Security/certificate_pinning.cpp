#include "fizmo_library.hpp"
#include "certificate_pinning.hpp"
#include "der.hpp"
#include "../../Crypto/hash.hpp"
#include "../../Crypto/encoding.hpp"

#include <algorithm>
#include <cctype>

namespace fizmo {
namespace networking {
namespace security {

namespace {

std::string lower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    while (!out.empty() && out.back() == '.') out.pop_back();
    return out;
}

} // namespace

PinSet::Rule& PinSet::rule_for(const std::string& host, bool include_subdomains) {
    const std::string pattern = lower(host);

    for (auto& r : m_rules) {
        if (r.pattern == pattern) {
            r.include_subdomains = r.include_subdomains || include_subdomains;
            return r;
        }
    }

    Rule r;
    r.pattern = pattern;
    r.include_subdomains = include_subdomains;
    m_rules.push_back(std::move(r));
    return m_rules.back();
}

PinSet& PinSet::add_pin(const std::string& host, const std::string& sha256_base64, bool include_subdomains) {
    std::string b64 = sha256_base64;
    const std::string prefix = "sha256/";
    if (b64.compare(0, prefix.size(), prefix) == 0) b64 = b64.substr(prefix.size());
    std::vector<std::uint8_t> raw;
    if (!crypto::base64_decode(b64, raw) || raw.size() != 32) throw std::invalid_argument("PinSet: pin must be a base64 SHA-256 digest");
    PinDigest d;
    std::copy(raw.begin(), raw.end(), d.begin());
    return add_pin(host, d, include_subdomains);
}

PinSet& PinSet::add_pin(const std::string& host, const PinDigest& digest, bool include_subdomains) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Rule& r = rule_for(host, include_subdomains);
    if (std::find(r.pins.begin(), r.pins.end(), digest) == r.pins.end()) r.pins.push_back(digest);
    return *this;
}

PinSet& PinSet::add_certificate(const std::string& host, const Certificate& cert, bool include_subdomains) {
    PinDigest d;
    if (!spki_sha256(cert, d)) throw std::invalid_argument("PinSet: certificate public key could not be extracted");
    return add_pin(host, d, include_subdomains);
}

PinSet& PinSet::set_expiry(const std::string& host, std::chrono::system_clock::time_point expires) {
    std::lock_guard<std::mutex> lock(m_mutex);
    rule_for(host, false).expires = expires;
    return *this;
}

PinSet& PinSet::remove_host(const std::string& host) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::string pattern = lower(host);
    m_rules.erase(std::remove_if(m_rules.begin(), m_rules.end(), [&](const Rule& r) { return r.pattern == pattern; }), m_rules.end());
    return *this;
}

void PinSet::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rules.clear();
}

std::size_t PinSet::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rules.size();
}

bool PinSet::host_matches(const Rule& rule, const std::string& host) {
    if (rule.pattern == host) return true;

    if (rule.pattern.size() > 2 && rule.pattern[0] == '*' && rule.pattern[1] == '.') {
        const std::string suffix = rule.pattern.substr(1);
        if (host.size() > suffix.size() && host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0) {
            const std::string label = host.substr(0, host.size() - suffix.size());
            if (label.find('.') == std::string::npos) return true;
            return rule.include_subdomains;
        }
    }

    if (rule.include_subdomains) {
        const std::string suffix = "." + rule.pattern;
        if (host.size() > suffix.size() && host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0) return true;
    }

    return false;
}

const PinSet::Rule* PinSet::match(const std::string& host) const {
    const std::string h = lower(host);
    const Rule* best = nullptr;
    const auto now = std::chrono::system_clock::now();

    for (const auto& r : m_rules) {
        if (r.expires <= now || r.pins.empty() || !host_matches(r, h)) continue;
        if (!best || r.pattern.size() > best->pattern.size()) best = &r;
    }

    return best;
}

bool PinSet::has_pins(const std::string& host) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return match(host) != nullptr;
}

bool PinSet::check_digests(const std::string& host, const std::vector<PinDigest>& presented, PinViolation* violation) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const Rule* r = match(host);
    if (!r) return true;

    for (const auto& p : presented) {
        if (std::find(r->pins.begin(), r->pins.end(), p) != r->pins.end()) return true;
    }

    if (violation) {
        violation->host = host;
        violation->presented.clear();
        violation->expected.clear();
        for (const auto& p : presented) violation->presented.push_back("sha256/" + crypto::base64_encode(p.data(), p.size()));
        for (const auto& p : r->pins) violation->expected.push_back("sha256/" + crypto::base64_encode(p.data(), p.size()));
    }

    return false;
}

bool PinSet::check(const std::string& host, const CertificateChain& chain, PinViolation* violation) const {
    std::vector<PinDigest> presented;

    for (std::size_t i = 0; i < chain.length(); ++i) {
        PinDigest d;
        if (spki_sha256(chain[i], d)) presented.push_back(d);
    }

    return check_digests(host, presented, violation);
}

bool PinSet::check(const std::string& host, const Certificate& leaf, PinViolation* violation) const {
    std::vector<PinDigest> presented;
    PinDigest d;
    if (spki_sha256(leaf, d)) presented.push_back(d);
    return check_digests(host, presented, violation);
}

bool PinSet::spki_sha256(const std::vector<std::uint8_t>& cert_der, PinDigest& out) {
    std::vector<std::uint8_t> spki;
    if (!der::certificate_spki(cert_der.data(), cert_der.size(), spki)) return false;
    out = crypto::SHA256::hash(spki.data(), spki.size());
    return true;
}

bool PinSet::spki_sha256(const Certificate& cert, PinDigest& out) {
    return cert.loaded() && spki_sha256(cert.raw_der(), out);
}

std::string PinSet::spki_sha256_base64(const Certificate& cert) {
    PinDigest d;
    if (!spki_sha256(cert, d)) return {};
    return crypto::base64_encode(d.data(), d.size());
}

VerifyResult PinningVerifier::verify(const std::string& hostname, const Certificate& leaf, const CertificateChain* chain) const noexcept {
    VerifyResult base = CertificateVerifier::verify(hostname, leaf, chain);
    if (!base.passed || !m_pins || hostname.empty()) return base;

    try {
        PinViolation violation;
        const bool ok = chain ? m_pins->check(hostname, *chain, &violation) : m_pins->check(hostname, leaf, &violation);
        if (ok) return base;
        if (m_on_violation) m_on_violation(violation);
        if (m_report_only) return base;
        return VerifyResult::failure(VerifyError::PinViolation, 0, "Certificate public key does not match any pin for \"" + hostname + "\"");
    } catch (...) {
        return VerifyResult::failure(VerifyError::PinViolation, 0, "Pin check failed");
    }
}

} // namespace security
} // namespace networking
} // namespace fizmo
