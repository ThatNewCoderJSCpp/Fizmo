#ifndef FIZMO_CERTIFICATE_CHAIN_HPP
#define FIZMO_CERTIFICATE_CHAIN_HPP

#include "certificate.hpp"
#include "../../Basic/basic_includes.hpp"
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
            const auto& current = m_certs[i];
            const auto& next    = m_certs[i + 1];

            if (!issuer_matches_subject(current, next)) {
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
            if (i == m_certs.size() - 1) { s += "}\n"; }
        }

        return s;
    }

private:
    std::vector<Certificate> m_certs;

    void parse_pem_bundle(const std::string& pem) noexcept {
        static const std::string BEGIN_MARKER = "-----BEGIN CERTIFICATE-----";
        static const std::string END_MARKER   = "-----END CERTIFICATE-----";
        std::size_t pos = 0;

        while (pos < pem.size()) {
            std::size_t begin = pem.find(BEGIN_MARKER, pos);
            if (begin == std::string::npos) break;
            std::size_t end = pem.find(END_MARKER, begin);
            if (end == std::string::npos) break;
            end += END_MARKER.size();

            if (end < pem.size() && (pem[end] == '\n' || pem[end] == '\r')) {
                ++end;
                if (end < pem.size() && pem[end] == '\n') ++end;
            }

            std::string block = pem.substr(begin, end - begin);
            Certificate cert;
            if (cert.load_from_pem(block)) { m_certs.push_back(std::move(cert)); }
            pos = end;
        }
    }

    static bool issuer_matches_subject(const Certificate& current, const Certificate& next) noexcept {
    #ifdef OS_WINDOWS
        PCCERT_CONTEXT cur_ctx  = current.native_handle();
        PCCERT_CONTEXT next_ctx = next.native_handle();

        if (cur_ctx && next_ctx) {
            return CertCompareCertificateName(
                X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                &cur_ctx->pCertInfo->Issuer,
                &next_ctx->pCertInfo->Subject
            ) == TRUE;
        }
    #endif

        const auto& issuer  = current.issuer();
        const auto& subject = next.subject();
        if (issuer.size() != subject.size()) return false;

        for (std::size_t i = 0; i < issuer.size(); ++i) {
            char a = issuer[i];
            char b = subject[i];
            if (a >= 'A' && a <= 'Z') a += ('a' - 'A');
            if (b >= 'A' && b <= 'Z') b += ('a' - 'A');
            if (a != b) return false;
        }

        return true;
    }
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CERTIFICATE_CHAIN_HPP