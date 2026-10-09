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

    static CertificateChain from_pem_bundle(const std::string& path) noexcept;

    static CertificateChain from_pem_string(const std::string& pem) noexcept {
        CertificateChain chain;
        if (!pem.empty()) chain.parse_pem_bundle(pem);
        return chain;
    }

    static CertificateChain from_der_list(const std::vector<std::vector<std::uint8_t>>& ders) noexcept;

    bool push_back(Certificate&& cert) noexcept {
        if (!cert.loaded()) return false;
        m_certs.push_back(std::move(cert));
        return true;
    }

    bool        empty()  const noexcept { return m_certs.empty(); }
    std::size_t length() const noexcept { return m_certs.size(); }
    const Certificate& leaf() const noexcept { return m_certs.front(); }
    const Certificate& root() const noexcept { return m_certs.back(); }

    std::vector<const Certificate*> intermediates() const noexcept;

    std::vector<const Certificate*> tail() const noexcept;

    const Certificate& operator[](std::size_t index) const noexcept { return m_certs[index]; }

    struct OrderResult {
        bool        valid = false;
        std::size_t break_index = 0;
        std::string detail;
        explicit operator bool() const noexcept { return valid; }
    };

    OrderResult validate_ordering() const noexcept;

    std::string to_string() const;

private:
    std::vector<Certificate> m_certs;

    void parse_pem_bundle(const std::string& pem) noexcept;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CERTIFICATE_CHAIN_HPP