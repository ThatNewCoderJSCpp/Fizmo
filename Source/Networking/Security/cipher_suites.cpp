#include "fizmo_library.hpp"
#include "cipher_suites.hpp"

#include <algorithm>
#include <cctype>

namespace fizmo {
namespace networking {
namespace security {

namespace {

using CS = CipherSuite;
using St = CipherStrength;

const std::vector<CipherSuiteInfo>& table() {
    static const std::vector<CipherSuiteInfo> t = {
        { CS::TLS_AES_128_GCM_SHA256,                        "TLS_AES_128_GCM_SHA256",                        "TLS_AES_128_GCM_SHA256",          true,  true,  true,  St::Modern },
        { CS::TLS_AES_256_GCM_SHA384,                        "TLS_AES_256_GCM_SHA384",                        "TLS_AES_256_GCM_SHA384",          true,  true,  true,  St::Modern },
        { CS::TLS_CHACHA20_POLY1305_SHA256,                  "TLS_CHACHA20_POLY1305_SHA256",                  "TLS_CHACHA20_POLY1305_SHA256",    true,  true,  true,  St::Modern },
        { CS::TLS_AES_128_CCM_SHA256,                        "TLS_AES_128_CCM_SHA256",                        "TLS_AES_128_CCM_SHA256",          true,  true,  true,  St::Secure },
        { CS::TLS_AES_128_CCM_8_SHA256,                      "TLS_AES_128_CCM_8_SHA256",                      "TLS_AES_128_CCM_8_SHA256",        true,  true,  true,  St::Legacy },
        { CS::TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,       "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",       "ECDHE-ECDSA-AES128-GCM-SHA256",   false, true,  true,  St::Modern },
        { CS::TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,         "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",         "ECDHE-RSA-AES128-GCM-SHA256",     false, true,  true,  St::Modern },
        { CS::TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,       "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",       "ECDHE-ECDSA-AES256-GCM-SHA384",   false, true,  true,  St::Modern },
        { CS::TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,         "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",         "ECDHE-RSA-AES256-GCM-SHA384",     false, true,  true,  St::Modern },
        { CS::TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256", "ECDHE-ECDSA-CHACHA20-POLY1305",   false, true,  true,  St::Modern },
        { CS::TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,   "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256",   "ECDHE-RSA-CHACHA20-POLY1305",     false, true,  true,  St::Modern },
        { CS::TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,           "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256",           "DHE-RSA-AES128-GCM-SHA256",       false, true,  true,  St::Secure },
        { CS::TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,           "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384",           "DHE-RSA-AES256-GCM-SHA384",       false, true,  true,  St::Secure },
        { CS::TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256,     "TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256",     "DHE-RSA-CHACHA20-POLY1305",       false, true,  true,  St::Secure },
        { CS::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,       "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256",       "ECDHE-ECDSA-AES128-SHA256",       false, true,  false, St::Legacy },
        { CS::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,       "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384",       "ECDHE-ECDSA-AES256-SHA384",       false, true,  false, St::Legacy },
        { CS::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,         "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",         "ECDHE-RSA-AES128-SHA256",         false, true,  false, St::Legacy },
        { CS::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,         "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384",         "ECDHE-RSA-AES256-SHA384",         false, true,  false, St::Legacy },
        { CS::TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,          "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA",          "ECDHE-ECDSA-AES128-SHA",          false, true,  false, St::Legacy },
        { CS::TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,          "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",          "ECDHE-ECDSA-AES256-SHA",          false, true,  false, St::Legacy },
        { CS::TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,            "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",            "ECDHE-RSA-AES128-SHA",            false, true,  false, St::Legacy },
        { CS::TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,            "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",            "ECDHE-RSA-AES256-SHA",            false, true,  false, St::Legacy },
        { CS::TLS_RSA_WITH_AES_128_GCM_SHA256,               "TLS_RSA_WITH_AES_128_GCM_SHA256",               "AES128-GCM-SHA256",               false, false, true,  St::Legacy },
        { CS::TLS_RSA_WITH_AES_256_GCM_SHA384,               "TLS_RSA_WITH_AES_256_GCM_SHA384",               "AES256-GCM-SHA384",               false, false, true,  St::Legacy },
        { CS::TLS_RSA_WITH_AES_128_CBC_SHA,                  "TLS_RSA_WITH_AES_128_CBC_SHA",                  "AES128-SHA",                      false, false, false, St::Legacy },
        { CS::TLS_RSA_WITH_AES_256_CBC_SHA,                  "TLS_RSA_WITH_AES_256_CBC_SHA",                  "AES256-SHA",                      false, false, false, St::Legacy },
        { CS::TLS_RSA_WITH_3DES_EDE_CBC_SHA,                 "TLS_RSA_WITH_3DES_EDE_CBC_SHA",                 "DES-CBC3-SHA",                    false, false, false, St::Weak },
        { CS::TLS_RSA_WITH_RC4_128_SHA,                      "TLS_RSA_WITH_RC4_128_SHA",                      "RC4-SHA",                         false, false, false, St::Weak },
        { CS::TLS_RSA_WITH_RC4_128_MD5,                      "TLS_RSA_WITH_RC4_128_MD5",                      "RC4-MD5",                         false, false, false, St::Weak },
        { CS::TLS_RSA_WITH_DES_CBC_SHA,                      "TLS_RSA_WITH_DES_CBC_SHA",                      "DES-CBC-SHA",                     false, false, false, St::Weak },
        { CS::TLS_RSA_WITH_NULL_SHA256,                      "TLS_RSA_WITH_NULL_SHA256",                      "NULL-SHA256",                     false, false, false, St::Weak },
        { CS::TLS_RSA_WITH_NULL_SHA,                         "TLS_RSA_WITH_NULL_SHA",                         "NULL-SHA",                        false, false, false, St::Weak },
        { CS::TLS_RSA_WITH_NULL_MD5,                         "TLS_RSA_WITH_NULL_MD5",                         "NULL-MD5",                        false, false, false, St::Weak },
        { CS::TLS_RSA_EXPORT_WITH_RC4_40_MD5,                "TLS_RSA_EXPORT_WITH_RC4_40_MD5",                "EXP-RC4-MD5",                     false, false, false, St::Weak }
    };

    return t;
}

std::string upper(const std::string& s) {
    std::string u = s;
    for (auto& c : u) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return u;
}

} // namespace

const std::vector<CipherSuiteInfo>& all_cipher_suites() noexcept { return table(); }

const CipherSuiteInfo* cipher_suite_info(CipherSuite suite) noexcept {
    for (const auto& i : table()) if (i.suite == suite) return &i;
    return nullptr;
}

const CipherSuiteInfo* cipher_suite_info(const std::string& name) noexcept {
    try {
        const std::string u = upper(name);

        for (const auto& i : table()) {
            if (u == i.iana_name || u == upper(i.openssl_name)) return &i;
        }
    } catch (...) {}

    return nullptr;
}

const char* cipher_strength_to_string(CipherStrength s) noexcept {
    switch (s) {
        case CipherStrength::Weak:   return "Weak";
        case CipherStrength::Legacy: return "Legacy";
        case CipherStrength::Secure: return "Secure";
        case CipherStrength::Modern: return "Modern";
    }

    return "Unknown";
}

CipherList::CipherList(std::initializer_list<CipherSuite> suites) {
    for (auto s : suites) add(s);
}

CipherList CipherList::default_secure_list() {
    CipherList l;
    for (const auto& i : table()) if (i.strength >= CipherStrength::Secure && i.suite != CipherSuite::TLS_AES_128_CCM_SHA256) l.add(i.suite);
    return l;
}

CipherList CipherList::modern_list() {
    CipherList l;
    for (const auto& i : table()) if (i.strength == CipherStrength::Modern) l.add(i.suite);
    return l;
}

CipherList CipherList::compatible_list() {
    CipherList l = default_secure_list();

    for (const auto& i : table()) {
        if (i.strength == CipherStrength::Legacy && i.suite != CipherSuite::TLS_AES_128_CCM_8_SHA256) l.add(i.suite);
    }

    return l;
}

CipherList CipherList::tls13_only() {
    return { CipherSuite::TLS_AES_256_GCM_SHA384, CipherSuite::TLS_CHACHA20_POLY1305_SHA256, CipherSuite::TLS_AES_128_GCM_SHA256 };
}

CipherList CipherList::from_names(const std::vector<std::string>& names, std::vector<std::string>* unknown) {
    CipherList l;

    for (const auto& n : names) {
        const auto* i = cipher_suite_info(n);
        if (i) l.add(i->suite);
        else if (unknown) unknown->push_back(n);
    }

    return l;
}

CipherList& CipherList::add(CipherSuite suite) {
    if (!contains(suite)) m_suites.push_back(suite);
    return *this;
}

CipherList& CipherList::remove(CipherSuite suite) {
    m_suites.erase(std::remove(m_suites.begin(), m_suites.end(), suite), m_suites.end());
    return *this;
}

CipherList& CipherList::remove_weak() {
    return remove_below(CipherStrength::Legacy);
}

CipherList& CipherList::remove_below(CipherStrength minimum) {
    m_suites.erase(std::remove_if(m_suites.begin(), m_suites.end(), [minimum](CipherSuite s) {
        const auto* i = cipher_suite_info(s);
        return !i || i->strength < minimum;
    }), m_suites.end());
    return *this;
}

bool CipherList::contains(CipherSuite suite) const noexcept {
    return std::find(m_suites.begin(), m_suites.end(), suite) != m_suites.end();
}

bool CipherList::has_weak() const noexcept {
    for (auto s : m_suites) {
        const auto* i = cipher_suite_info(s);
        if (!i || i->strength == CipherStrength::Weak) return true;
    }

    return false;
}

std::vector<std::string> CipherList::iana_names() const {
    std::vector<std::string> out;
    for (auto s : m_suites) if (const auto* i = cipher_suite_info(s)) out.push_back(i->iana_name);
    return out;
}

std::vector<std::string> CipherList::openssl_names() const {
    std::vector<std::string> out;
    for (auto s : m_suites) if (const auto* i = cipher_suite_info(s)) out.push_back(i->openssl_name);
    return out;
}

std::vector<std::string> CipherList::native_names() const {
#if defined(OS_WINDOWS)
    return iana_names();
#else
    return openssl_names();
#endif
}

std::vector<unsigned> CipherList::to_schannel_algs() const {
    std::vector<unsigned> algs;
    auto add = [&](unsigned a) { if (std::find(algs.begin(), algs.end(), a) == algs.end()) algs.push_back(a); };

    for (auto s : m_suites) {
        const auto* i = cipher_suite_info(s);
        if (!i || i->tls13) continue;
        const std::string n = i->iana_name;
        if (n.find("ECDHE") != std::string::npos) add(0xAE06);
        else if (n.find("DHE") != std::string::npos) add(0xAA02);
        else add(0xA400);
        if (n.find("ECDSA") != std::string::npos) add(0x2203);
        else add(0x2400);
        if (n.find("AES_128") != std::string::npos) add(0x660E);
        if (n.find("AES_256") != std::string::npos) add(0x6610);
        if (n.find("3DES") != std::string::npos) add(0x6603);
        if (n.find("RC4") != std::string::npos) add(0x6801);
        if (n.find("_DES_") != std::string::npos) add(0x6601);
        if (n.find("SHA384") != std::string::npos) add(0x800D);
        else if (n.find("SHA256") != std::string::npos) add(0x800C);
        else if (n.find("_SHA") != std::string::npos) add(0x8004);
        if (n.find("MD5") != std::string::npos) add(0x8003);
    }

    return algs;
}

std::string CipherList::to_string() const {
    std::string s;

    for (auto c : m_suites) {
        const auto* i = cipher_suite_info(c);
        if (!i) continue;
        if (!s.empty()) s += ':';
        s += i->iana_name;
    }

    return s;
}

} // namespace security
} // namespace networking
} // namespace fizmo
