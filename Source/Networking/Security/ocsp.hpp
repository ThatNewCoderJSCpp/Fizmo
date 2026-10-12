#ifndef FIZMO_OCSP_HPP
#define FIZMO_OCSP_HPP

#include "certificate.hpp"
#include "certificate_chain.hpp"
#include "Backend/native_backend.hpp"

#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace security {

class OCSPVerifier {
public:
    void set_ca_store_path(const std::string& path) { m_ca_store_path = path; }
    const std::string& ca_store_path() const noexcept { return m_ca_store_path; }

    void set_require_staple(bool require) noexcept { m_require = require; }
    bool require_staple() const noexcept { return m_require; }

    OCSPResult check(const std::vector<std::uint8_t>& response, const Certificate& leaf, const Certificate& issuer) const noexcept;
    OCSPResult check(const std::vector<std::uint8_t>& response, const CertificateChain& chain) const noexcept;

    VerifyResult to_verify_result(const OCSPResult& result) const noexcept;

private:
    std::string m_ca_store_path;
    bool        m_require = false;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_OCSP_HPP
