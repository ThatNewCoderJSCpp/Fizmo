#ifndef FIZMO_CERTIFICATE_VERIFIER_HPP
#define FIZMO_CERTIFICATE_VERIFIER_HPP

#include "certificate.hpp"
#include "tls_context.hpp"
#include "../../Basic/basic_includes.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace fizmo {
namespace networking {
namespace security {

enum class VerifyError : std::uint8_t {
    None = 0,
    Expired,
    UntrustedRoot,
    HostnameMismatch,
    Revoked,
    SelfSigned,
    WeakSignature
};

inline const char* verify_error_to_string(VerifyError e) noexcept {
    switch (e) {
        case VerifyError::None:             return "None";
        case VerifyError::Expired:          return "Expired";
        case VerifyError::UntrustedRoot:    return "UntrustedRoot";
        case VerifyError::HostnameMismatch: return "HostnameMismatch";
        case VerifyError::Revoked:          return "Revoked";
        case VerifyError::SelfSigned:       return "SelfSigned";
        case VerifyError::WeakSignature:    return "WeakSignature";
        default:                            return "Unknown";
    }
}

struct VerifyResult {
    bool         passed     = false;
    VerifyError  error      = VerifyError::None;
    int          depth      = -1;        
    std::string  detail;                 

    explicit operator bool() const noexcept { return passed; }

    static VerifyResult success() noexcept { return { true, VerifyError::None, -1, {} }; }
    static VerifyResult failure(VerifyError err, int chain_depth = 0, const std::string& msg = {}) noexcept { return { false, err, chain_depth, msg }; }

    std::string to_string() const {
        std::string s = passed ? "PASS" : "FAIL";

        if (!passed) {
            s += " error=";
            s += verify_error_to_string(error);
            
            if (depth >= 0) {
                s += " depth=";
                s += std::to_string(depth);
            }

            if (!detail.empty()) {
                s += " (";
                s += detail;
                s += ')';
            }
        }

        return s;
    }
};

class CertificateVerifier {
public:
    CertificateVerifier()  noexcept = default;
    virtual ~CertificateVerifier() noexcept = default;
    CertificateVerifier(const CertificateVerifier&)            = delete;
    CertificateVerifier& operator=(const CertificateVerifier&) = delete;
    CertificateVerifier(CertificateVerifier&&)                 = default;
    CertificateVerifier& operator=(CertificateVerifier&&)      = default;

    virtual VerifyResult verify(const std::string& hostname, const Certificate& leaf) const noexcept {
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

        if (leaf.is_self_signed()) {
            return VerifyResult::failure(VerifyError::SelfSigned, 0, "Leaf certificate is self-signed");
        }

        return verify_chain_platform(hostname, leaf);
    }

#ifdef OS_WINDOWS
    virtual VerifyResult verify(const std::string& hostname, PCCERT_CONTEXT ctx) const noexcept {
        if (!ctx) {
            return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "Null certificate context");
        }

        return verify_chain_schannel(hostname, ctx);
    }
#endif

protected:
    virtual VerifyResult verify_chain_platform(const std::string& hostname, const Certificate& leaf) const noexcept {
    #ifdef OS_WINDOWS
        PCCERT_CONTEXT ctx = leaf.native_handle();

        if (!ctx) {
            return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "Certificate has no native handle");
        }

        return verify_chain_schannel(hostname, ctx);
    #else
        (void)hostname; 
        (void)leaf;
        
        return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "Platform verification not implemented");
    #endif
    }

#ifdef OS_WINDOWS
private:
    VerifyResult verify_chain_schannel(const std::string& hostname, PCCERT_CONTEXT ctx) const noexcept {
        CERT_ENHKEY_USAGE        enhkey_usage  = {};
        enhkey_usage.cUsageIdentifier           = 0;
        enhkey_usage.rgpszUsageIdentifier       = nullptr;
        CERT_USAGE_MATCH         usage_match   = {};
        usage_match.dwType                      = USAGE_MATCH_TYPE_AND;
        usage_match.Usage                       = enhkey_usage;
        CERT_CHAIN_PARA          chain_params  = {};
        chain_params.cbSize                     = sizeof(chain_params);
        chain_params.RequestedUsage             = usage_match;
        PCCERT_CHAIN_CONTEXT chain_ctx = nullptr;

        BOOL ok = CertGetCertificateChain(
            nullptr,                    
            ctx,                        
            nullptr,                    
            ctx->hCertStore,            
            &chain_params,
            CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT,
            nullptr,                   
            &chain_ctx
        );

        if (!ok || !chain_ctx) {
            return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "CertGetCertificateChain failed");
        }

        VerifyResult result = inspect_chain_status(chain_ctx);
        
        if (!result.passed) {
            CertFreeCertificateChain(chain_ctx);
            return result;
        }

        std::wstring wide_host = utf8_to_wide(hostname);
        SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_policy = {};
        ssl_policy.cbSize        = sizeof(ssl_policy);
        ssl_policy.dwAuthType    = AUTHTYPE_SERVER;
        ssl_policy.fdwChecks     = 0;
        ssl_policy.pwszServerName = wide_host.empty() ? nullptr : const_cast<WCHAR*>(wide_host.c_str());
        CERT_CHAIN_POLICY_PARA  policy_params = {};
        policy_params.cbSize       = sizeof(policy_params);
        policy_params.pvExtraPolicyPara = &ssl_policy;
        CERT_CHAIN_POLICY_STATUS policy_status = {};
        policy_status.cbSize = sizeof(policy_status);

        ok = CertVerifyCertificateChainPolicy(
            CERT_CHAIN_POLICY_SSL,
            chain_ctx,
            &policy_params,
            &policy_status
        );

        CertFreeCertificateChain(chain_ctx);

        if (!ok) {
            return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "CertVerifyCertificateChainPolicy call failed");
        }

        if (policy_status.dwError != ERROR_SUCCESS) {
            return map_policy_error(policy_status.dwError, static_cast<int>(policy_status.lChainIndex));
        }

        return VerifyResult::success();
    }

    static VerifyResult inspect_chain_status(PCCERT_CHAIN_CONTEXT chain_ctx) noexcept {
        const DWORD flags = chain_ctx->TrustStatus.dwErrorStatus;
        if (flags == CERT_TRUST_NO_ERROR) { return VerifyResult::success(); }

        if (flags & CERT_TRUST_IS_REVOKED) {
            int d = error_depth(chain_ctx, CERT_TRUST_IS_REVOKED);
            return VerifyResult::failure(VerifyError::Revoked, d, "Certificate revoked");
        }

        if (flags & CERT_TRUST_IS_NOT_TIME_VALID) {
            int d = error_depth(chain_ctx, CERT_TRUST_IS_NOT_TIME_VALID);
            return VerifyResult::failure(VerifyError::Expired, d, "Certificate not time-valid");
        }

        if (flags & CERT_TRUST_IS_UNTRUSTED_ROOT) {
            int d = chain_length(chain_ctx) - 1;
            return VerifyResult::failure(VerifyError::UntrustedRoot, d, "Root certificate is not trusted");
        }

        if (flags & CERT_TRUST_IS_PARTIAL_CHAIN) {
            return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "Incomplete certificate chain");
        }

        if (flags & (CERT_TRUST_IS_NOT_SIGNATURE_VALID | CERT_TRUST_HAS_WEAK_SIGNATURE)) {
            int d = error_depth(chain_ctx, CERT_TRUST_IS_NOT_SIGNATURE_VALID | CERT_TRUST_HAS_WEAK_SIGNATURE);
            return VerifyResult::failure(VerifyError::WeakSignature, d, "Signature invalid or weak algorithm");
        }

        if (flags & CERT_TRUST_IS_NOT_VALID_FOR_USAGE) {
            return VerifyResult::failure(VerifyError::WeakSignature, 0, "Key usage constraint violated");
        }

        return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "Chain trust status 0x" + to_hex(flags));
    }

    static VerifyResult map_policy_error(DWORD err, int depth) noexcept {
        switch (err) {
            case static_cast<DWORD>(CERT_E_EXPIRED):
            case static_cast<DWORD>(CERT_E_VALIDITYPERIODNESTING):
                return VerifyResult::failure(VerifyError::Expired, depth);

            case static_cast<DWORD>(CERT_E_UNTRUSTEDROOT):
            case static_cast<DWORD>(CERT_E_UNTRUSTEDTESTROOT):
            case static_cast<DWORD>(CERT_E_CHAINING):
                return VerifyResult::failure(VerifyError::UntrustedRoot, depth);

            case static_cast<DWORD>(CERT_E_WRONG_USAGE):
                return VerifyResult::failure(VerifyError::WeakSignature, depth, "Key usage mismatch");

            case static_cast<DWORD>(CERT_E_REVOKED):
                return VerifyResult::failure(VerifyError::Revoked, depth);

            case static_cast<DWORD>(CERT_E_CN_NO_MATCH):
                return VerifyResult::failure(VerifyError::HostnameMismatch, depth);

            default:
                return VerifyResult::failure(VerifyError::UntrustedRoot, depth, "Policy error 0x" + to_hex(err));
        }
    }

    static int error_depth(PCCERT_CHAIN_CONTEXT chain_ctx, DWORD flag) noexcept {
        if (!chain_ctx || chain_ctx->cChain == 0) return 0;
        const auto& simple = chain_ctx->rgpChain[0];
        
        for (DWORD i = 0; i < simple->cElement; ++i) {
            if (simple->rgpElement[i]->TrustStatus.dwErrorStatus & flag) { return static_cast<int>(i); }
        }

        return 0;
    }

    static int chain_length(PCCERT_CHAIN_CONTEXT chain_ctx) noexcept {
        if (!chain_ctx || chain_ctx->cChain == 0) return 0;
        return static_cast<int>(chain_ctx->rgpChain[0]->cElement);
    }

    static std::wstring utf8_to_wide(const std::string& utf8) noexcept {
        if (utf8.empty()) return {};

        int needed = MultiByteToWideChar(
            CP_UTF8, 
            MB_ERR_INVALID_CHARS,
            utf8.data(), 
            static_cast<int>(utf8.size()),
            nullptr, 
            0
        );

        if (needed <= 0) return {};
        std::wstring wide(static_cast<std::size_t>(needed), L'\0');
        
        int written = MultiByteToWideChar(
            CP_UTF8, 
            MB_ERR_INVALID_CHARS,
            utf8.data(), 
            static_cast<int>(utf8.size()),
            &wide[0], 
            needed
        );

        if (written <= 0) return {};
        wide.resize(static_cast<std::size_t>(written));
        return wide;
    }

    static std::string to_hex(DWORD val) {
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%08lX", static_cast<unsigned long>(val));
        return buf;
    }
#endif // OS_WINDOWS
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CERTIFICATE_VERIFIER_HPP