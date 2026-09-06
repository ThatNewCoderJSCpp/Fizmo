#ifndef FIZMO_CERTIFICATE_CHAIN_HPP
#define FIZMO_CERTIFICATE_CHAIN_HPP

#include "certificate.hpp"
#include "Backend/native_backend.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <algorithm>

namespace fizmo {
namespace networking {
namespace security {

class CertificateChain {
public:
    CertificateChain() noexcept = default;
    ~CertificateChain() noexcept = default;
    CertificateChain(const CertificateChain&)            = delete;
    CertificateChain& operator=(const CertificateChain&) = delete;
    CertificateChain(CertificateChain&&)                 = default;
    CertificateChain& operator=(CertificateChain&&)      = default;

    static CertificateChain from_pem_bundle(const std::string& path) noexcept {
        CertificateChain chain;
        std::ifstream file(path, std::ios::binary);
        if (!file) return chain;
        std::string pem((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (pem.empty()) return chain;
        chain.parse_pem_bundle(pem);
        return chain;
    }

    static CertificateChain from_pem_string(const std::string& pem) noexcept {
        CertificateChain chain;
        if (!pem.empty()) chain.parse_pem_bundle(pem);
        return chain;
    }

    static CertificateChain from_der_list(const std::vector<std::vector<std::uint8_t>>& ders) noexcept {
        CertificateChain chain;

        for (const auto& der : ders) {
            Certificate cert;
            if (cert.load_from_der(der)) { chain.m_certs.push_back(std::move(cert)); }
        }

        return chain;
    }

    bool push_back(Certificate&& cert) noexcept {
        if (!cert.loaded()) return false;
        m_certs.push_back(std::move(cert));
        return true;
    }

    bool        empty()  const noexcept { return m_certs.empty(); }
    std::size_t length() const noexcept { return m_certs.size(); }
    const Certificate& leaf() const noexcept { return m_certs.front(); }
    const Certificate& root() const noexcept { return m_certs.back(); }

    std::vector<const Certificate*> intermediates() const noexcept {
        std::vector<const Certificate*> mids;
        if (m_certs.size() <= 2) return mids;
        mids.reserve(m_certs.size() - 2);
        for (std::size_t i = 1; i + 1 < m_certs.size(); ++i) { mids.push_back(&m_certs[i]); }
        return mids;
    }

    std::vector<const Certificate*> tail() const noexcept {
        std::vector<const Certificate*> rest;
        if (m_certs.size() <= 1) return rest;
        rest.reserve(m_certs.size() - 1);
        for (std::size_t i = 1; i < m_certs.size(); ++i) { rest.push_back(&m_certs[i]); }
        return rest;
    }

    const Certificate& operator[](std::size_t index) const noexcept { return m_certs[index]; }

    struct OrderResult {
        bool        valid = false;
        std::size_t break_index = 0;
        std::string detail;
        explicit operator bool() const noexcept { return valid; }
    };

    OrderResult validate_ordering() const noexcept {
        if (m_certs.empty()) { return { false, 0, "Chain is empty" }; }
        if (m_certs.size() == 1) { return { true, 0, {} }; }

        for (std::size_t i = 0; i + 1 < m_certs.size(); ++i) {
            if (!detail::NativeCertOps::issuer_links_to_subject(m_certs[i].native(), m_certs[i + 1].native())) {
                return {
                    false, i,
                    "Issuer of cert[" + std::to_string(i) +
                    "] does not match subject of cert[" +
                    std::to_string(i + 1) + "]"
                };
            }
        }

        return { true, 0, {} };
    }

    std::string to_string() const {
        if (m_certs.empty()) return "CertificateChain[empty]";
        std::string s = "CertificateChain[" + std::to_string(m_certs.size()) + "] {\n";

        for (std::size_t i = 0; i < m_certs.size(); ++i) {
            s += "    [" + std::to_string(i) + "]: ";
            s += m_certs[i].to_string();
            s += "\n";
        }

        s += "}\n";
        return s;
    }

private:
    std::vector<Certificate> m_certs;

    void parse_pem_bundle(const std::string& pem) noexcept {
        for (const auto& block : detail::PEM::split_bundle(pem)) {
            Certificate cert;
            if (cert.load_from_pem(block)) { m_certs.push_back(std::move(cert)); }
        }
    }
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CERTIFICATE_CHAIN_HPP