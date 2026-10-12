#include "fizmo_library.hpp"
#include "ocsp.hpp"
#include "certificate_store.hpp"

#include <filesystem>

namespace fizmo {
namespace networking {
namespace security {

OCSPResult OCSPVerifier::check(const std::vector<std::uint8_t>& response, const Certificate& leaf, const Certificate& issuer) const noexcept {
    if (response.empty()) {
        OCSPResult r;
        r.status = OCSPCertStatus::Unknown;
        r.detail = "No OCSP response was stapled";
        return r;
    }

    return detail::NativeCertOps::verify_ocsp(response, leaf.native(), issuer.loaded() ? &issuer.native() : nullptr, m_ca_store_path);
}

OCSPResult OCSPVerifier::check(const std::vector<std::uint8_t>& response, const CertificateChain& chain) const noexcept {
    if (chain.empty()) {
        OCSPResult r;
        r.status = OCSPCertStatus::Invalid;
        r.detail = "No certificate to check";
        return r;
    }

    if (chain.length() >= 2) return check(response, chain.leaf(), chain[1]);

    try {
        CertificateStore store;

        if (m_ca_store_path.empty()) {
            store = CertificateStore::open_system(CertificateStore::SystemStore::Root);
        } else {
            std::error_code ec;
            if (std::filesystem::is_directory(m_ca_store_path, ec)) store.load_directory(m_ca_store_path);
            else store.load_pem_file(m_ca_store_path);
        }

        const Certificate* issuer = store.find_issuer_of(chain.leaf());
        if (issuer) return check(response, chain.leaf(), *issuer);
    } catch (...) {}

    OCSPResult r;
    r.status = OCSPCertStatus::Invalid;
    r.detail = "The issuer certificate needed to check the OCSP response could not be found";
    return r;
}

VerifyResult OCSPVerifier::to_verify_result(const OCSPResult& result) const noexcept {
    switch (result.status) {
        case OCSPCertStatus::Good:
            return VerifyResult::success();
        case OCSPCertStatus::Revoked:
            return VerifyResult::failure(VerifyError::Revoked, 0, result.detail);
        case OCSPCertStatus::Unsupported:
            return VerifyResult::success();
        case OCSPCertStatus::Unknown:
            if (!m_require) return VerifyResult::success();
            return VerifyResult::failure(VerifyError::RevocationUnknown, 0, result.detail);
        case OCSPCertStatus::Invalid:
        default:
            return VerifyResult::failure(VerifyError::RevocationUnknown, 0, result.detail);
    }
}

} // namespace security
} // namespace networking
} // namespace fizmo
