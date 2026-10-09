#include "fizmo_library.hpp"
#include "certificate_verifier.hpp"

namespace fizmo {
namespace networking {
namespace security {

auto CertificateVerifier::verify(
    const std::string& hostname,
    const Certificate& leaf,
    const CertificateChain* chain
) const noexcept -> VerifyResult {
    if (!leaf.loaded()) {
        return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "No certificate presented");
    }

    if (!hostname.empty() && !leaf.matches_hostname(hostname)) {
        return VerifyResult::failure(
            VerifyError::HostnameMismatch, 0,
            "Hostname \"" + hostname + "\" does not match certificate"
        );
    }

    if (leaf.is_expired()) {
        return VerifyResult::failure(VerifyError::Expired, 0, "Leaf certificate expired");
    }

    if (!m_allow_self_signed && leaf.is_self_signed()) {
        return VerifyResult::failure(VerifyError::SelfSigned, 0, "Leaf certificate is self-signed");
    }

    return verify_chain_platform(hostname, leaf, chain);
}

auto CertificateVerifier::verify_chain_platform(
    const std::string& hostname,
    const Certificate& leaf,
    const CertificateChain* chain
) const noexcept -> VerifyResult {
    std::vector<const detail::NativeCertificateBase*> extras;

    if (chain) {
        for (const auto* cert : chain->tail()) {
            if (cert && cert->loaded()) { extras.push_back(&cert->native()); }
        }
    }

    return detail::NativeCertOps::verify_chain(
        leaf.native(),
        extras,
        hostname,
        m_ca_store_path,
        m_check_revocation
    );
}

} // namespace security
} // namespace networking
} // namespace fizmo
