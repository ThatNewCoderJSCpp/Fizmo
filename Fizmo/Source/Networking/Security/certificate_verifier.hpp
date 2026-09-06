#ifndef FIZMO_CERTIFICATE_VERIFIER_HPP
#define FIZMO_CERTIFICATE_VERIFIER_HPP

#include "certificate.hpp"
#include "certificate_chain.hpp"
#include "Backend/native_backend.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace fizmo {
namespace networking {
namespace security {

class CertificateVerifier {
public:
    CertificateVerifier() noexcept = default;
    virtual ~CertificateVerifier() noexcept = default;
    CertificateVerifier(const CertificateVerifier&)            = delete;
    CertificateVerifier& operator=(const CertificateVerifier&) = delete;
    CertificateVerifier(CertificateVerifier&&)                 = default;
    CertificateVerifier& operator=(CertificateVerifier&&)      = default;

public:
    void set_ca_store_path(const std::string& path) { m_ca_store_path = path; }
    const std::string& ca_store_path() const noexcept { return m_ca_store_path; }

    void set_check_revocation(bool enable) noexcept { m_check_revocation = enable; }
    bool check_revocation() const noexcept { return m_check_revocation; }

    void set_allow_self_signed(bool enable) noexcept { m_allow_self_signed = enable; }
    bool allow_self_signed() const noexcept { return m_allow_self_signed; }

public:
    virtual VerifyResult verify(const std::string& hostname, const Certificate& leaf) const noexcept {
        return verify(hostname, leaf, nullptr);
    }

    virtual VerifyResult verify(
        const std::string& hostname,
        const Certificate& leaf,
        const CertificateChain* chain
    ) const noexcept {
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

protected:
    virtual VerifyResult verify_chain_platform(
        const std::string& hostname,
        const Certificate& leaf,
        const CertificateChain* chain
    ) const noexcept {
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

private:
    std::string m_ca_store_path;
    bool        m_check_revocation  = true;
    bool        m_allow_self_signed = false;
};

class PermissiveCertificateVerifier : public CertificateVerifier {
public:
    VerifyResult verify(const std::string&, const Certificate&) const noexcept override {
        return VerifyResult::success();
    }

    VerifyResult verify(const std::string&, const Certificate&, const CertificateChain*) const noexcept override {
        return VerifyResult::success();
    }
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CERTIFICATE_VERIFIER_HPP