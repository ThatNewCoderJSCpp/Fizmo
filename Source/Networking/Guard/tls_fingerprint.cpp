#include "fizmo_library.hpp"
#include "tls_fingerprint.hpp"
#include "../../Crypto/hash.hpp"
#include "../../Crypto/encoding.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fizmo {
namespace networking {
namespace guard {

namespace {

std::string hex4(std::uint16_t v) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04x", static_cast<unsigned>(v));
    return buf;
}

std::string two_digits(std::size_t n) {
    if (n > 99) n = 99;
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%02u", static_cast<unsigned>(n));
    return buf;
}

std::string join_hex(const std::vector<std::uint16_t>& values) {
    std::string out;

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += ',';
        out += hex4(values[i]);
    }

    return out;
}

std::string truncated_sha256(const std::string& text) {
    if (text.empty()) return "000000000000";
    const auto d = crypto::SHA256::hash(text);
    return crypto::to_hex(d.data(), 6);
}

std::string version_code(const security::ClientHelloInfo& h) {
    std::uint16_t best = 0;

    for (std::uint16_t v : h.supported_versions) {
        if (!is_grease(v) && v > best) best = v;
    }

    if (!best) best = h.legacy_version;

    switch (best) {
        case 0x0304: return "13";
        case 0x0303: return "12";
        case 0x0302: return "11";
        case 0x0301: return "10";
        case 0x0300: return "s3";
        case 0x0002: return "s2";
        case 0xfeff: return "d1";
        case 0xfefd: return "d2";
        case 0xfefc: return "d3";
        default: return "00";
    }
}

std::string alpn_code(const std::vector<std::string>& alpn) {
    if (alpn.empty() || alpn.front().empty()) return "00";
    const std::string& a = alpn.front();
    const unsigned char first = static_cast<unsigned char>(a.front());
    const unsigned char last = static_cast<unsigned char>(a.back());

    if (std::isalnum(first) && std::isalnum(last)) {
        return std::string(1, static_cast<char>(first)) + static_cast<char>(last);
    }

    const std::string hex = crypto::to_hex(a.data(), a.size());
    return std::string(1, hex.front()) + hex.back();
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

} // namespace

bool is_grease(std::uint16_t value) noexcept {
    return (value & 0x0f0f) == 0x0a0a && (value >> 8) == (value & 0xff);
}

TLSFingerprint fingerprint(const security::ClientHelloInfo& hello, Transport transport) {
    TLSFingerprint fp;
    std::vector<std::uint16_t> ciphers;
    std::vector<std::uint16_t> exts;
    std::vector<std::uint16_t> exts_sorted;
    std::vector<std::uint16_t> sigs;

    for (std::uint16_t c : hello.cipher_suites) {
        if (is_grease(c)) fp.uses_grease = true;
        else ciphers.push_back(c);
    }

    for (std::uint16_t e : hello.extensions) {
        if (is_grease(e)) { fp.uses_grease = true; continue; }
        exts.push_back(e);
        if (e != 0x0000 && e != 0x0010) exts_sorted.push_back(e);
    }

    for (std::uint16_t s : hello.signature_algorithms) {
        if (!is_grease(s)) sigs.push_back(s);
    }

    fp.has_sni = !hello.server_name.empty() || std::find(exts.begin(), exts.end(), std::uint16_t(0)) != exts.end();
    fp.tls_version = version_code(hello);
    fp.alpn = hello.alpn_protocols.empty() ? std::string() : hello.alpn_protocols.front();
    fp.cipher_count = ciphers.size();
    fp.extension_count = exts.size();

    fp.ja4_a = std::string(1, static_cast<char>(transport)) + fp.tls_version + (fp.has_sni ? 'd' : 'i')
             + two_digits(ciphers.size()) + two_digits(exts.size()) + alpn_code(hello.alpn_protocols);

    std::vector<std::uint16_t> ciphers_sorted = ciphers;
    std::sort(ciphers_sorted.begin(), ciphers_sorted.end());
    std::sort(exts_sorted.begin(), exts_sorted.end());

    const std::string cipher_text = join_hex(ciphers_sorted);
    std::string ext_text = join_hex(exts_sorted);
    if (!sigs.empty()) ext_text += "_" + join_hex(sigs);

    fp.ja4_b = truncated_sha256(cipher_text);
    fp.ja4_c = exts_sorted.empty() ? std::string("000000000000") : truncated_sha256(ext_text);
    fp.ja4 = fp.ja4_a + "_" + fp.ja4_b + "_" + fp.ja4_c;
    fp.ja4_raw = fp.ja4_a + "_" + cipher_text + "_" + ext_text;
    return fp;
}

const char* to_string(ClientFamily family) noexcept {
    switch (family) {
        case ClientFamily::Chromium:   return "chromium";
        case ClientFamily::Firefox:    return "firefox";
        case ClientFamily::Safari:     return "safari";
        case ClientFamily::Automation: return "automation";
        case ClientFamily::Missing:    return "missing";
        default:                       return "unknown";
    }
}

ClientFamily classify_user_agent(const std::string& user_agent) {
    if (user_agent.empty()) return ClientFamily::Missing;
    const std::string ua = lower(user_agent);

    static const char* const tools[] = {
        "curl/", "wget/", "python-requests", "python-urllib", "aiohttp", "httpx", "go-http-client", "okhttp",
        "java/", "apache-httpclient", "libwww-perl", "scrapy", "node-fetch", "axios/", "postmanruntime",
        "headlesschrome", "phantomjs", "selenium", "puppeteer", "playwright", "httpie", "powershell"
    };

    for (const char* t : tools) {
        if (contains(ua, t)) return ClientFamily::Automation;
    }

    if (contains(ua, "firefox/") && !contains(ua, "seamonkey")) return ClientFamily::Firefox;
    if (contains(ua, "chrome/") || contains(ua, "chromium/") || contains(ua, "edg/") || contains(ua, "opr/") || contains(ua, "crios/")) return ClientFamily::Chromium;
    if (contains(ua, "safari/") && contains(ua, "version/")) return ClientFamily::Safari;
    return ClientFamily::Unknown;
}

const char* to_string(RiskLevel level) noexcept {
    switch (level) {
        case RiskLevel::Medium:  return "medium";
        case RiskLevel::High:    return "high";
        case RiskLevel::Blocked: return "blocked";
        default:                 return "low";
    }
}

FingerprintRegistry::FingerprintRegistry(FingerprintPolicy policy) : m_policy(policy) {}

void FingerprintRegistry::trust(const std::string& ja4, const std::string& label) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_blocked.erase(ja4);
    m_trusted[ja4] = label;
}

void FingerprintRegistry::block(const std::string& ja4, const std::string& label) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_trusted.erase(ja4);
    m_blocked[ja4] = label;
}

void FingerprintRegistry::forget(const std::string& ja4) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_trusted.erase(ja4);
    m_blocked.erase(ja4);
}

bool FingerprintRegistry::trusted(const std::string& ja4) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_trusted.count(ja4) != 0;
}

bool FingerprintRegistry::blocked(const std::string& ja4) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_blocked.count(ja4) != 0;
}

void FingerprintRegistry::set_policy(const FingerprintPolicy& policy) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_policy = policy;
}

void FingerprintRegistry::finish(FingerprintVerdict& v) const {
    if (v.level == RiskLevel::Blocked) return;
    if (v.score < 0) v.score = 0;
    if (v.score > 100) v.score = 100;
    if (v.score >= m_policy.high_score) v.level = RiskLevel::High;
    else if (v.score >= m_policy.medium_score) v.level = RiskLevel::Medium;
    else v.level = RiskLevel::Low;
}

FingerprintVerdict FingerprintRegistry::assess(const TLSFingerprint& fp, const std::string& user_agent) const {
    FingerprintVerdict v;
    v.claimed = classify_user_agent(user_agent);

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto b = m_blocked.find(fp.ja4);

        if (b != m_blocked.end()) {
            v.level = RiskLevel::Blocked;
            v.score = 100;
            v.label = b->second;
            v.reasons.push_back("fingerprint is blocked");
            return v;
        }

        const auto t = m_trusted.find(fp.ja4);

        if (t != m_trusted.end()) {
            v.label = t->second;
            v.reasons.push_back("fingerprint is trusted");
            finish(v);
            return v;
        }
    }

    if (fp.empty()) {
        v.score += 20;
        v.reasons.push_back("no TLS fingerprint");
    }

    const bool browser = v.claimed == ClientFamily::Chromium || v.claimed == ClientFamily::Firefox || v.claimed == ClientFamily::Safari;

    switch (v.claimed) {
        case ClientFamily::Missing:
            v.score += 25;
            v.reasons.push_back("no User-Agent");
            break;
        case ClientFamily::Automation:
            v.score += 45;
            v.reasons.push_back("User-Agent is an automation tool");
            break;
        case ClientFamily::Unknown:
            v.score += 10;
            v.reasons.push_back("unrecognised User-Agent");
            break;
        default:
            break;
    }

    if (!fp.empty() && browser) {
        if (v.claimed == ClientFamily::Chromium && !fp.uses_grease) {
            v.score += 40;
            v.reasons.push_back("claims Chromium but sends no GREASE values");
        }

        if (fp.tls_version != "13") {
            v.score += 25;
            v.reasons.push_back("claims a modern browser but does not offer TLS 1.3");
        }

        if (fp.alpn != "h2" && fp.alpn != "h3") {
            v.score += 20;
            v.reasons.push_back("claims a browser but does not offer HTTP/2 first");
        }

        if (!fp.has_sni) {
            v.score += 10;
            v.reasons.push_back("claims a browser but sends no SNI");
        }

        if (fp.cipher_count < 8 || fp.extension_count < 8) {
            v.score += 15;
            v.reasons.push_back("ClientHello is smaller than browsers send");
        }
    }

    finish(v);
    return v;
}

FingerprintVerdict FingerprintRegistry::assess_plaintext(const std::string& user_agent) const {
    FingerprintVerdict v = assess(TLSFingerprint(), user_agent);

    if (m_policy.require_tls && v.level != RiskLevel::Blocked) {
        v.level = RiskLevel::Blocked;
        v.reasons.push_back("plain HTTP is not allowed");
    }

    return v;
}

} // namespace guard
} // namespace networking
} // namespace fizmo
