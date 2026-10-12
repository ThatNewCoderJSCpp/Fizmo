#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "schannel_backend.hpp"
#include "pem_utils.hpp"
#include "../der.hpp"

namespace fizmo {
namespace networking {
namespace security {
namespace detail {

#if defined(OS_WINDOWS)
std::wstring utf8_to_wide(const std::string& utf8) noexcept {
    if (utf8.empty()) return {};

    int needed = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        utf8.data(), static_cast<int>(utf8.size()),
        nullptr, 0
    );

    if (needed <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');

    int written = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        utf8.data(), static_cast<int>(utf8.size()),
        &wide[0], needed
    );

    if (written <= 0) return {};
    wide.resize(static_cast<std::size_t>(written));
    return wide;
}
#endif

#if defined(OS_WINDOWS)
std::string wide_to_utf8(const wchar_t* wide) noexcept {
    if (!wide) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<std::size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &out[0], needed, nullptr, nullptr);
    return out;
}
#endif

#if defined(OS_WINDOWS)
std::string hex32(unsigned long value) {
    char buf[12];
    std::snprintf(buf, sizeof(buf), "%08lX", value);
    return buf;
}
#endif

#if defined(OS_WINDOWS)
SchannelCertificate::~SchannelCertificate() noexcept { release(); }
#endif

#if defined(OS_WINDOWS)
SchannelCertificate::SchannelCertificate(SchannelCertificate&& other) noexcept : m_ctx(other.m_ctx), m_info(std::move(other.m_info)) {
    other.m_ctx = nullptr;
}
#endif

#if defined(OS_WINDOWS)
auto SchannelCertificate::operator=(SchannelCertificate&& other) noexcept -> SchannelCertificate& {
    if (this != &other) {
        release();
        m_ctx = other.m_ctx;
        m_info = std::move(other.m_info);
        other.m_ctx = nullptr;
    }

    return *this;
}
#endif

#if defined(OS_WINDOWS)
bool SchannelCertificate::parse(const std::uint8_t* der, std::size_t len) noexcept {
    release();
    if (!der || len == 0) return false;

    m_ctx = CertCreateCertificateContext(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        der,
        static_cast<DWORD>(len)
    );

    if (!m_ctx) return false;
    fill_info();
    return true;
}
#endif

#if defined(OS_WINDOWS)
void SchannelCertificate::release() noexcept {
    if (m_ctx) { CertFreeCertificateContext(m_ctx); m_ctx = nullptr; }
    m_info.clear();
}
#endif

#if defined(OS_WINDOWS)
bool SchannelCertificate::valid() const noexcept { return m_ctx != nullptr; }
#endif

#if defined(OS_WINDOWS)
auto SchannelCertificate::info() const noexcept -> const CertificateInfo& { return m_info; }
#endif

#if defined(OS_WINDOWS)
void* SchannelCertificate::native() const noexcept { return const_cast<void*>(static_cast<const void*>(m_ctx)); }
#endif

#if defined(OS_WINDOWS)
bool SchannelCertificate::self_signed() const noexcept {
    if (!m_ctx) return false;
    return CertCompareCertificateName(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        &m_ctx->pCertInfo->Subject,
        &m_ctx->pCertInfo->Issuer
    ) == TRUE;
}
#endif

#if defined(OS_WINDOWS)
bool SchannelCertificate::adopt(PCCERT_CONTEXT ctx) noexcept {
    release();
    if (!ctx) return false;
    m_ctx = CertDuplicateCertificateContext(ctx);
    if (!m_ctx) return false;
    fill_info();
    return true;
}
#endif

#if defined(OS_WINDOWS)
void SchannelCertificate::fill_info() noexcept {
    m_info.clear();
    m_info.subject = name_string(CERT_NAME_SIMPLE_DISPLAY_TYPE, 0);
    m_info.issuer  = name_string(CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG);
    m_info.serial  = format_serial(m_ctx->pCertInfo->SerialNumber);
    m_info.not_before = filetime_to_unix(m_ctx->pCertInfo->NotBefore);
    m_info.not_after  = filetime_to_unix(m_ctx->pCertInfo->NotAfter);
    parse_sans();
    parse_key_usage();
}
#endif

#if defined(OS_WINDOWS)
std::string SchannelCertificate::name_string(DWORD type, DWORD flags) const noexcept {
    DWORD needed = CertGetNameStringA(m_ctx, type, flags, nullptr, nullptr, 0);
    if (needed <= 1) return {};
    std::string out(static_cast<std::size_t>(needed), '\0');
    DWORD written = CertGetNameStringA(m_ctx, type, flags, nullptr, &out[0], needed);
    if (written <= 1) return {};
    out.resize(static_cast<std::size_t>(written - 1));
    return out;
}
#endif

#if defined(OS_WINDOWS)
std::string SchannelCertificate::format_serial(const CRYPT_INTEGER_BLOB& blob) {
    std::string result;
    result.reserve(static_cast<std::size_t>(blob.cbData) * 3);

    for (DWORD i = blob.cbData; i > 0; --i) {
        char hex[4];
        std::snprintf(hex, sizeof(hex), "%02X", blob.pbData[i - 1]);
        if (!result.empty()) result += ':';
        result += hex;
    }

    return result;
}
#endif

#if defined(OS_WINDOWS)
std::int64_t SchannelCertificate::filetime_to_unix(const FILETIME& ft) noexcept {
    ULARGE_INTEGER ull;
    ull.LowPart  = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    auto secs_since_1601 = static_cast<std::int64_t>(ull.QuadPart / 10000000ULL);
    return secs_since_1601 - 11644473600LL;
}
#endif

#if defined(OS_WINDOWS)
void SchannelCertificate::parse_sans() noexcept {
    PCERT_EXTENSION ext = CertFindExtension(
        szOID_SUBJECT_ALT_NAME2,
        m_ctx->pCertInfo->cExtension,
        m_ctx->pCertInfo->rgExtension
    );

    if (!ext) {
        ext = CertFindExtension(
            szOID_SUBJECT_ALT_NAME,
            m_ctx->pCertInfo->cExtension,
            m_ctx->pCertInfo->rgExtension
        );
    }

    if (!ext) return;
    PCERT_ALT_NAME_INFO alt = nullptr;
    DWORD alt_size = 0;

    if (!CryptDecodeObjectEx(
            X509_ASN_ENCODING,
            X509_ALTERNATE_NAME,
            ext->Value.pbData,
            ext->Value.cbData,
            CRYPT_DECODE_ALLOC_FLAG,
            nullptr,
            &alt,
            &alt_size
        )
    ) {
        return;
    }

    if (!alt) return;

    for (DWORD i = 0; i < alt->cAltEntry; ++i) {
        const auto& entry = alt->rgAltEntry[i];

        if (entry.dwAltNameChoice == CERT_ALT_NAME_DNS_NAME && entry.pwszDNSName) {
            std::string dns = wide_to_utf8(entry.pwszDNSName);
            if (!dns.empty()) { m_info.sans.push_back({ SubjectAltName::Type::DNS, std::move(dns) }); }
        } else if (entry.dwAltNameChoice == CERT_ALT_NAME_IP_ADDRESS) {
            const auto& blob = entry.IPAddress;

            if (blob.cbData == 4) {
                char addr[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, blob.pbData, addr, sizeof(addr));
                m_info.sans.push_back({ SubjectAltName::Type::IP, addr });
            } else if (blob.cbData == 16) {
                char addr[INET6_ADDRSTRLEN]{};
                inet_ntop(AF_INET6, blob.pbData, addr, sizeof(addr));
                m_info.sans.push_back({ SubjectAltName::Type::IP, addr });
            }
        }
    }

    LocalFree(alt);
}
#endif

#if defined(OS_WINDOWS)
void SchannelCertificate::parse_key_usage() noexcept {
    PCERT_EXTENSION ext = CertFindExtension(
        szOID_KEY_USAGE,
        m_ctx->pCertInfo->cExtension,
        m_ctx->pCertInfo->rgExtension
    );

    if (!ext) return;
    CRYPT_BIT_BLOB* bits = nullptr;
    DWORD bits_size = 0;

    if (!CryptDecodeObjectEx(
            X509_ASN_ENCODING,
            X509_KEY_USAGE,
            ext->Value.pbData,
            ext->Value.cbData,
            CRYPT_DECODE_ALLOC_FLAG,
            nullptr,
            &bits,
            &bits_size
        )
    ) {
        return;
    }

    if (!bits || bits->cbData == 0) { if (bits) LocalFree(bits); return; }
    BYTE b0 = bits->pbData[0];
    m_info.key_usage.digital_signature = (b0 & CERT_DIGITAL_SIGNATURE_KEY_USAGE) != 0;
    m_info.key_usage.non_repudiation   = (b0 & CERT_NON_REPUDIATION_KEY_USAGE)   != 0;
    m_info.key_usage.key_encipherment  = (b0 & CERT_KEY_ENCIPHERMENT_KEY_USAGE)  != 0;
    m_info.key_usage.data_encipherment = (b0 & CERT_DATA_ENCIPHERMENT_KEY_USAGE) != 0;
    m_info.key_usage.key_agreement     = (b0 & CERT_KEY_AGREEMENT_KEY_USAGE)     != 0;
    m_info.key_usage.cert_sign         = (b0 & CERT_KEY_CERT_SIGN_KEY_USAGE)     != 0;
    m_info.key_usage.crl_sign          = (b0 & CERT_OFFLINE_CRL_SIGN_KEY_USAGE)  != 0;
    m_info.key_usage.encipher_only     = (b0 & CERT_ENCIPHER_ONLY_KEY_USAGE)     != 0;
    if (bits->cbData > 1) { m_info.key_usage.decipher_only = (bits->pbData[1] & CERT_DECIPHER_ONLY_KEY_USAGE) != 0; }
    LocalFree(bits);
}
#endif

#if defined(OS_WINDOWS)
bool SchannelCertOps::issuer_links_to_subject(
    const NativeCertificateBase& child,
    const NativeCertificateBase& issuer
) noexcept {
    auto c = static_cast<PCCERT_CONTEXT>(child.native());
    auto i = static_cast<PCCERT_CONTEXT>(issuer.native());
    if (!c || !i) return false;

    return CertCompareCertificateName(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        &c->pCertInfo->Issuer,
        &i->pCertInfo->Subject
    ) == TRUE;
}
#endif

#if defined(OS_WINDOWS)
auto SchannelCertOps::verify_chain(
    const NativeCertificateBase& leaf,
    const std::vector<const NativeCertificateBase*>& extras,
    const std::string& hostname,
    const std::string& ca_store_path,
    bool check_revocation,
    const std::vector<std::string>& crl_paths
) noexcept -> VerifyResult {
    auto leaf_ctx = static_cast<PCCERT_CONTEXT>(leaf.native());
    if (!leaf_ctx) { return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "No leaf certificate"); }
    HCERTSTORE roots = nullptr;
    HCERTSTORE extra_store = build_extra_store(extras, ca_store_path, &roots);
    HCERTCHAINENGINE engine = nullptr;

    if (roots) {
        CERT_CHAIN_ENGINE_CONFIG cfg;
        std::memset(&cfg, 0, sizeof(cfg));
        cfg.cbSize = sizeof(cfg);
        cfg.hExclusiveRoot = roots;
        if (!CertCreateCertificateChainEngine(&cfg, &engine)) engine = nullptr;
    }

    CERT_ENHKEY_USAGE enhkey_usage = {};
    CERT_USAGE_MATCH  usage_match  = {};
    usage_match.dwType = USAGE_MATCH_TYPE_AND;
    usage_match.Usage  = enhkey_usage;
    CERT_CHAIN_PARA chain_params = {};
    chain_params.cbSize         = sizeof(chain_params);
    chain_params.RequestedUsage = usage_match;
    DWORD chain_flags = check_revocation ? CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT : 0;
    PCCERT_CHAIN_CONTEXT chain_ctx = nullptr;

    BOOL ok = CertGetCertificateChain(
        engine,
        leaf_ctx,
        nullptr,
        extra_store ? extra_store : leaf_ctx->hCertStore,
        &chain_params,
        chain_flags,
        nullptr,
        &chain_ctx
    );

    auto cleanup = [&]() {
        if (chain_ctx) CertFreeCertificateChain(chain_ctx);
        if (engine) CertFreeCertificateChainEngine(engine);
        if (extra_store) CertCloseStore(extra_store, 0);
        if (roots) CertCloseStore(roots, 0);
    };

    if (!ok || !chain_ctx) {
        cleanup();
        return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "CertGetCertificateChain failed");
    }

    const bool anchored = (chain_ctx->TrustStatus.dwErrorStatus & CERT_TRUST_IS_UNTRUSTED_ROOT) && root_in_store(chain_ctx, roots);
    VerifyResult result = inspect_chain_status(chain_ctx, anchored ? static_cast<DWORD>(CERT_TRUST_IS_UNTRUSTED_ROOT) : 0u);

    if (!result.passed) {
        const bool only_offline = result.error == VerifyError::RevocationUnknown && !crl_paths.empty();
        if (!only_offline) { cleanup(); return result; }
    }

    if (!crl_paths.empty() && check_revocation) {
        VerifyResult crl = check_crls(chain_ctx, crl_paths);
        if (!crl.passed) { cleanup(); return crl; }
    }

    std::wstring wide_host = utf8_to_wide(hostname);
    SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_policy = {};
    ssl_policy.cbSize         = sizeof(ssl_policy);
    ssl_policy.dwAuthType     = AUTHTYPE_SERVER;
    ssl_policy.fdwChecks      = check_revocation ? 0 : 0x00000080;
    if (!crl_paths.empty()) ssl_policy.fdwChecks |= 0x00000080;
    if (anchored) ssl_policy.fdwChecks |= 0x00000100;
    ssl_policy.pwszServerName = wide_host.empty() ? nullptr : const_cast<WCHAR*>(wide_host.c_str());
    CERT_CHAIN_POLICY_PARA policy_params = {};
    policy_params.cbSize            = sizeof(policy_params);
    policy_params.pvExtraPolicyPara = &ssl_policy;
    CERT_CHAIN_POLICY_STATUS policy_status = {};
    policy_status.cbSize = sizeof(policy_status);

    ok = CertVerifyCertificateChainPolicy(
        CERT_CHAIN_POLICY_SSL,
        chain_ctx,
        &policy_params,
        &policy_status
    );

    cleanup();

    if (!ok) {
        return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "CertVerifyCertificateChainPolicy call failed");
    }

    if (policy_status.dwError != ERROR_SUCCESS) {
        return map_policy_error(policy_status.dwError, static_cast<int>(policy_status.lChainIndex));
    }

    return VerifyResult::success();
}
#endif

#if defined(OS_WINDOWS)
int SchannelCertOps::load_file_into_store(const std::string& path, HCERTSTORE certs, HCERTSTORE crls) noexcept {
    std::FILE* f = _wfopen(utf8_to_wide(path).c_str(), L"rb");
    if (!f) return -1;
    std::vector<std::uint8_t> data;
    std::uint8_t buf[8192];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) data.insert(data.end(), buf, buf + n);
    std::fclose(f);
    int added = 0;

    if (PEM::looks_like_pem(data.data(), data.size()) || std::string(data.begin(), data.end()).find("-----BEGIN") != std::string::npos) {
        try {
            for (const auto& block : PEM::decode_all(std::string(data.begin(), data.end()))) {
                if (block.label == "CERTIFICATE" || block.label == "TRUSTED CERTIFICATE") {
                    if (certs && CertAddEncodedCertificateToStore(certs, X509_ASN_ENCODING, block.der.data(), static_cast<DWORD>(block.der.size()), CERT_STORE_ADD_USE_EXISTING, nullptr)) ++added;
                } else if (block.label == "X509 CRL") {
                    if (crls && CertAddEncodedCRLToStore(crls, X509_ASN_ENCODING, block.der.data(), static_cast<DWORD>(block.der.size()), CERT_STORE_ADD_NEWER, nullptr)) ++added;
                }
            }
        } catch (...) {
            return -1;
        }

        return added;
    }

    if (certs && CertAddEncodedCertificateToStore(certs, X509_ASN_ENCODING, data.data(), static_cast<DWORD>(data.size()), CERT_STORE_ADD_USE_EXISTING, nullptr)) return 1;
    if (crls && CertAddEncodedCRLToStore(crls, X509_ASN_ENCODING, data.data(), static_cast<DWORD>(data.size()), CERT_STORE_ADD_NEWER, nullptr)) return 1;
    return 0;
}
#endif

#if defined(OS_WINDOWS)
auto SchannelCertOps::build_extra_store(
    const std::vector<const NativeCertificateBase*>& extras,
    const std::string& ca_store_path,
    HCERTSTORE* roots
) noexcept -> HCERTSTORE {
    if (roots) *roots = nullptr;
    if (extras.empty() && ca_store_path.empty()) return nullptr;
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, nullptr);
    if (!store) return nullptr;

    for (const auto* cert : extras) {
        if (!cert) continue;
        auto ctx = static_cast<PCCERT_CONTEXT>(cert->native());
        if (!ctx) continue;
        CertAddCertificateContextToStore(store, ctx, CERT_STORE_ADD_USE_EXISTING, nullptr);
    }

    if (!ca_store_path.empty()) {
        HCERTSTORE root_store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, nullptr);

        if (root_store && load_file_into_store(ca_store_path, root_store, nullptr) > 0) {
            PCCERT_CONTEXT c = nullptr;
            while ((c = CertEnumCertificatesInStore(root_store, c)) != nullptr) CertAddCertificateContextToStore(store, c, CERT_STORE_ADD_USE_EXISTING, nullptr);
            if (roots) *roots = root_store;
            else CertCloseStore(root_store, 0);
        } else {
            if (root_store) CertCloseStore(root_store, 0);
            std::wstring wide = utf8_to_wide(ca_store_path);
            HCERTSTORE sys = CertOpenSystemStoreW(0, wide.c_str());

            if (sys) {
                PCCERT_CONTEXT c = nullptr;
                while ((c = CertEnumCertificatesInStore(sys, c)) != nullptr) CertAddCertificateContextToStore(store, c, CERT_STORE_ADD_USE_EXISTING, nullptr);
                CertCloseStore(sys, 0);
            }
        }
    }

    return store;
}
#endif

#if defined(OS_WINDOWS)
auto SchannelCertOps::check_crls(PCCERT_CHAIN_CONTEXT chain_ctx, const std::vector<std::string>& crl_paths) noexcept -> VerifyResult {
    if (!chain_ctx || chain_ctx->cChain == 0) return VerifyResult::success();
    HCERTSTORE crls = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, nullptr);
    if (!crls) return VerifyResult::failure(VerifyError::RevocationUnknown, 0, "Could not create CRL store");

    for (const auto& path : crl_paths) {
        if (load_file_into_store(path, nullptr, crls) <= 0) {
            CertCloseStore(crls, 0);
            return VerifyResult::failure(VerifyError::RevocationUnknown, 0, "Could not load CRL \"" + path + "\"");
        }
    }

    const CERT_SIMPLE_CHAIN* simple = chain_ctx->rgpChain[0];

    for (DWORD i = 0; i + 1 < simple->cElement; ++i) {
        PCCERT_CONTEXT cert   = simple->rgpElement[i]->pCertContext;
        PCCERT_CONTEXT issuer = simple->rgpElement[i + 1]->pCertContext;
        PCCRL_CONTEXT crl = nullptr;

        while ((crl = CertEnumCRLsInStore(crls, crl)) != nullptr) {
            if (!CertCompareCertificateName(X509_ASN_ENCODING, &cert->pCertInfo->Issuer, &crl->pCrlInfo->Issuer)) continue;

            if (!CryptVerifyCertificateSignatureEx(0, X509_ASN_ENCODING, CRYPT_VERIFY_CERT_SIGN_SUBJECT_CRL, const_cast<CRL_CONTEXT*>(crl),
                                                   CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT, const_cast<CERT_CONTEXT*>(issuer), 0, nullptr)) {
                continue;
            }

            PCRL_ENTRY entry = nullptr;

            if (CertFindCertificateInCRL(cert, crl, 0, nullptr, &entry) && entry) {
                CertFreeCRLContext(crl);
                CertCloseStore(crls, 0);
                return VerifyResult::failure(VerifyError::Revoked, static_cast<int>(i), "Certificate is listed in a CRL");
            }
        }
    }

    CertCloseStore(crls, 0);
    return VerifyResult::success();
}
#endif

#if defined(OS_WINDOWS)
bool SchannelCertOps::root_in_store(PCCERT_CHAIN_CONTEXT chain_ctx, HCERTSTORE roots) noexcept {
    if (!roots || !chain_ctx || chain_ctx->cChain != 1) return false;
    PCERT_SIMPLE_CHAIN simple = chain_ctx->rgpChain[0];
    if (!simple || simple->cElement == 0) return false;
    PCCERT_CONTEXT top = simple->rgpElement[simple->cElement - 1]->pCertContext;
    if (!top) return false;
    PCCERT_CONTEXT hit = CertFindCertificateInStore(roots, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_EXISTING, top, nullptr);
    if (!hit) return false;
    const bool same = hit->cbCertEncoded == top->cbCertEncoded && std::memcmp(hit->pbCertEncoded, top->pbCertEncoded, top->cbCertEncoded) == 0;
    CertFreeCertificateContext(hit);
    return same;
}
#endif

#if defined(OS_WINDOWS)
auto SchannelCertOps::inspect_chain_status(PCCERT_CHAIN_CONTEXT chain_ctx, DWORD ignored) noexcept -> VerifyResult {
    const DWORD flags = chain_ctx->TrustStatus.dwErrorStatus & ~ignored;
    if (flags == CERT_TRUST_NO_ERROR) { return VerifyResult::success(); }

    if (flags & CERT_TRUST_IS_REVOKED) {
        return VerifyResult::failure(VerifyError::Revoked, error_depth(chain_ctx, CERT_TRUST_IS_REVOKED), "Certificate revoked");
    }

    if (flags & CERT_TRUST_IS_NOT_TIME_VALID) {
        return VerifyResult::failure(VerifyError::Expired, error_depth(chain_ctx, CERT_TRUST_IS_NOT_TIME_VALID), "Certificate not time-valid");
    }

    if (flags & CERT_TRUST_IS_UNTRUSTED_ROOT) {
        return VerifyResult::failure(VerifyError::UntrustedRoot, chain_length(chain_ctx) - 1, "Root certificate is not trusted");
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

    if (flags & (CERT_TRUST_REVOCATION_STATUS_UNKNOWN | CERT_TRUST_IS_OFFLINE_REVOCATION)) {
        return VerifyResult::failure(VerifyError::RevocationUnknown, 0, "Revocation status unavailable");
    }

    return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "Chain trust status 0x" + hex32(flags));
}
#endif

#if defined(OS_WINDOWS)
auto SchannelCertOps::map_policy_error(DWORD err, int depth) noexcept -> VerifyResult {
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

        case static_cast<DWORD>(CRYPT_E_NO_REVOCATION_CHECK):
        case static_cast<DWORD>(CRYPT_E_REVOCATION_OFFLINE):
            return VerifyResult::failure(VerifyError::RevocationUnknown, depth);

        case static_cast<DWORD>(CERT_E_CN_NO_MATCH):
            return VerifyResult::failure(VerifyError::HostnameMismatch, depth);

        default:
            return VerifyResult::failure(VerifyError::UntrustedRoot, depth, "Policy error 0x" + hex32(err));
    }
}
#endif

#if defined(OS_WINDOWS)
int SchannelCertOps::error_depth(PCCERT_CHAIN_CONTEXT chain_ctx, DWORD flag) noexcept {
    if (!chain_ctx || chain_ctx->cChain == 0) return 0;
    const auto& simple = chain_ctx->rgpChain[0];

    for (DWORD i = 0; i < simple->cElement; ++i) {
        if (simple->rgpElement[i]->TrustStatus.dwErrorStatus & flag) { return static_cast<int>(i); }
    }

    return 0;
}
#endif

#if defined(OS_WINDOWS)
int SchannelCertOps::chain_length(PCCERT_CHAIN_CONTEXT chain_ctx) noexcept {
    if (!chain_ctx || chain_ctx->cChain == 0) return 0;
    return static_cast<int>(chain_ctx->rgpChain[0]->cElement);
}
#endif

#if defined(OS_WINDOWS)
SchannelCredentials::SchannelCredentials() noexcept { std::memset(&m_cred, 0, sizeof(m_cred)); }
#endif

#if defined(OS_WINDOWS)
SchannelCredentials::~SchannelCredentials() noexcept { release(); }
#endif

#if defined(OS_WINDOWS)
namespace {

struct FzUnicodeString {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
};

struct FzCryptoSettings {
    int             eAlgorithmUsage;
    FzUnicodeString strCngAlgId;
    DWORD           cChainingModes;
    FzUnicodeString* rgstrChainingModes;
    DWORD           dwMinBitLength;
    DWORD           dwMaxBitLength;
};

struct FzTlsParameters {
    DWORD             cAlpnIds;
    FzUnicodeString*  rgstrAlpnIds;
    DWORD             grbitDisabledProtocols;
    DWORD             cDisabledCrypto;
    FzCryptoSettings* pDisabledCrypto;
    DWORD             dwFlags;
};

struct FzSchCredentials {
    DWORD            dwVersion;
    DWORD            dwCredFormat;
    DWORD            cCreds;
    PCCERT_CONTEXT*  paCred;
    HCERTSTORE       hRootStore;
    DWORD            cMappers;
    void**           aphMappers;
    DWORD            dwSessionLifespan;
    DWORD            dwFlags;
    DWORD            cTlsParameters;
    FzTlsParameters* pTlsParameters;
};

FzUnicodeString make_ustr(const wchar_t* w) noexcept {
    FzUnicodeString u;
    const std::size_t n = std::wcslen(w);
    u.Length = static_cast<USHORT>(n * sizeof(wchar_t));
    u.MaximumLength = static_cast<USHORT>((n + 1) * sizeof(wchar_t));
    u.Buffer = const_cast<PWSTR>(w);
    return u;
}

bool any_suite(const std::vector<std::string>& suites, const char* needle, const char* exclude = nullptr) {
    for (const auto& s : suites) {
        std::string u = s;
        for (auto& c : u) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (u.find(needle) == std::string::npos) continue;
        if (exclude && u.find(exclude) != std::string::npos) continue;
        return true;
    }

    return false;
}

} // namespace

bool SchannelCredentials::acquire(const TLSConfig& config) noexcept {
    release();
    m_last_error.clear();

    if (!config.allow_insecure_versions && static_cast<std::uint32_t>(config.min_version) < static_cast<std::uint32_t>(TLSVersion::TLS_1_2)) {
        m_last_error = "Insecure minimum protocol version requested (below TLS 1.2); set allow_insecure_versions to override";
        return false;
    }

    if (static_cast<std::uint32_t>(config.min_version) > static_cast<std::uint32_t>(config.max_version)) {
        m_last_error = "Minimum protocol version is greater than the maximum";
        return false;
    }

    try { m_config = config; } catch (...) { m_last_error = "Out of memory"; return false; }

    if (!open_certificate_store(config)) {
        if (m_last_error.empty()) m_last_error = "Certificate store could not be opened";
        return false;
    }

    DWORD flags = 0;

    switch (config.verify_mode) {
        case VerifyMode::None:
            flags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_SERVERNAME_CHECK;
            break;
        case VerifyMode::Peer:
        case VerifyMode::FailIfNoPeer:
            flags = SCH_CRED_MANUAL_CRED_VALIDATION;
            if (config.check_revocation) { flags |= SCH_CRED_REVOCATION_CHECK_CHAIN; }
            break;
    }

    if (config.use == ContextUse::Client) flags |= SCH_CRED_NO_DEFAULT_CREDS;
    if (!config.session_tickets && config.use == ContextUse::Server) flags |= 0x00010000;
    const unsigned long use = (config.use == ContextUse::Server) ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND;
    if (config.native_algorithms.empty() && acquire_modern(flags | SCH_USE_STRONG_CRYPTO, use)) return true;
    return acquire_legacy(flags, use);
}

bool SchannelCredentials::acquire_modern(DWORD flags, unsigned long use) noexcept {
    const TLSConfig& config = m_config;
    FzSchCredentials cred;
    std::memset(&cred, 0, sizeof(cred));
    cred.dwVersion = 5;
    cred.dwFlags = flags;
    cred.dwSessionLifespan = config.session_timeout_seconds > 0 ? config.session_timeout_seconds * 1000u : 0u;

    if (m_cert_ctx) {
        cred.cCreds = 1;
        cred.paCred = &m_cert_ctx;
    }

    const DWORD known = SP_PROT_TLS1_0 | SP_PROT_TLS1_1 | SP_PROT_TLS1_2 | SP_PROT_TLS1_3;
    FzTlsParameters params;
    std::memset(&params, 0, sizeof(params));
    params.grbitDisabledProtocols = known & ~protocol_flags(config);
    FzUnicodeString cbc_mode = make_ustr(L"ChainingModeCBC");
    std::vector<FzCryptoSettings> disabled;

    if (!config.cipher_suites.empty()) {
        const auto& suites = config.cipher_suites;
        auto add = [&](int usage, const wchar_t* alg, DWORD minb, DWORD maxb, FzUnicodeString* modes, DWORD nmodes) {
            FzCryptoSettings c;
            std::memset(&c, 0, sizeof(c));
            c.eAlgorithmUsage = usage;
            c.strCngAlgId = make_ustr(alg);
            c.cChainingModes = nmodes;
            c.rgstrChainingModes = modes;
            c.dwMinBitLength = minb;
            c.dwMaxBitLength = maxb;
            disabled.push_back(c);
        };

        if (!any_suite(suites, "CBC")) add(2, L"AES", 0, 0, &cbc_mode, 1);
        if (!any_suite(suites, "CHACHA20")) add(2, L"CHACHA20_POLY1305", 0, 0, nullptr, 0);
        if (!any_suite(suites, "AES_128") && !any_suite(suites, "AES128") && (any_suite(suites, "AES_256") || any_suite(suites, "AES256"))) add(2, L"AES", 128, 128, nullptr, 0);
        if (!any_suite(suites, "AES_256") && !any_suite(suites, "AES256") && (any_suite(suites, "AES_128") || any_suite(suites, "AES128"))) add(2, L"AES", 256, 256, nullptr, 0);
        if (!any_suite(suites, "DHE", "ECDHE")) add(0, L"DH", 0, 0, nullptr, 0);
        if (!any_suite(suites, "TLS_RSA_WITH") && !any_suite(suites, "RSA_WITH_AES", "DHE")) add(0, L"RSA", 0, 0, nullptr, 0);
    }

    params.cDisabledCrypto = static_cast<DWORD>(disabled.size());
    params.pDisabledCrypto = disabled.empty() ? nullptr : disabled.data();
    cred.cTlsParameters = 1;
    cred.pTlsParameters = &params;
    TimeStamp expiry;

    SECURITY_STATUS status = AcquireCredentialsHandleW(
        nullptr, const_cast<SEC_WCHAR*>(UNISP_NAME_W), use, nullptr, &cred, nullptr, nullptr, &m_cred, &expiry
    );

    m_acquired = (status == SEC_E_OK);
    if (!m_acquired) m_last_error = "AcquireCredentialsHandle (SCH_CREDENTIALS) failed 0x" + hex32(static_cast<unsigned long>(status));
    return m_acquired;
}

bool SchannelCredentials::acquire_legacy(DWORD flags, unsigned long use) noexcept {
    const TLSConfig& config = m_config;
    SCHANNEL_CRED cred;
    std::memset(&cred, 0, sizeof(cred));
    cred.dwVersion             = SCHANNEL_CRED_VERSION;
    cred.grbitEnabledProtocols = protocol_flags(config);
    cred.dwFlags               = flags;
    cred.dwSessionLifespan     = config.session_timeout_seconds > 0 ? config.session_timeout_seconds * 1000u : 0u;
    std::vector<ALG_ID> algs(config.native_algorithms.begin(), config.native_algorithms.end());

    if (!algs.empty()) {
        cred.cSupportedAlgs = static_cast<DWORD>(algs.size());
        cred.palgSupportedAlgs = algs.data();
    }

    if (m_cert_ctx) {
        cred.cCreds = 1;
        cred.paCred = &m_cert_ctx;
    }

    TimeStamp expiry;

    SECURITY_STATUS status = AcquireCredentialsHandleW(
        nullptr, const_cast<SEC_WCHAR*>(UNISP_NAME_W), use, nullptr, &cred, nullptr, nullptr, &m_cred, &expiry
    );

    if (status != SEC_E_OK && (cred.grbitEnabledProtocols & SP_PROT_TLS1_3)) {
        cred.grbitEnabledProtocols &= ~static_cast<DWORD>(SP_PROT_TLS1_3);
        if (cred.grbitEnabledProtocols != 0) status = AcquireCredentialsHandleW(nullptr, const_cast<SEC_WCHAR*>(UNISP_NAME_W), use, nullptr, &cred, nullptr, nullptr, &m_cred, &expiry);
    }

    m_acquired = (status == SEC_E_OK);
    if (!m_acquired) { m_last_error = "AcquireCredentialsHandle failed 0x" + hex32(static_cast<unsigned long>(status)); }
    return m_acquired;
}
#endif

#if defined(OS_WINDOWS)
void SchannelCredentials::release() noexcept {
    if (m_acquired) {
        FreeCredentialsHandle(&m_cred);
        std::memset(&m_cred, 0, sizeof(m_cred));
        m_acquired = false;
    }

    if (m_cert_ctx) { CertFreeCertificateContext(m_cert_ctx); m_cert_ctx = nullptr; }
    if (m_cert_store) { CertCloseStore(m_cert_store, 0); m_cert_store = nullptr; }
}
#endif

#if defined(OS_WINDOWS)
bool SchannelCredentials::acquired() const noexcept { return m_acquired; }
#endif

#if defined(OS_WINDOWS)
const std::string& SchannelCredentials::last_error() const noexcept { return m_last_error; }
#endif

#if defined(OS_WINDOWS)
void* SchannelCredentials::native() const noexcept { return const_cast<CredHandle*>(&m_cred); }
#endif

#if defined(OS_WINDOWS)
auto SchannelCredentials::protocol_flags(const TLSConfig& config) noexcept -> DWORD {
    DWORD flags = 0;
    auto lo = static_cast<std::uint32_t>(config.min_version);
    auto hi = static_cast<std::uint32_t>(config.max_version);
    if (lo <= 0x0301 && hi >= 0x0301) flags |= SP_PROT_TLS1_0;
    if (lo <= 0x0302 && hi >= 0x0302) flags |= SP_PROT_TLS1_1;
    if (lo <= 0x0303 && hi >= 0x0303) flags |= SP_PROT_TLS1_2;
#ifdef SP_PROT_TLS1_3
    if (lo <= 0x0304 && hi >= 0x0304) flags |= SP_PROT_TLS1_3;
#endif
    return flags;
}
#endif

#if defined(OS_WINDOWS)
bool SchannelCredentials::open_certificate_store(const TLSConfig& config) noexcept {
    if (config.certificate.empty()) return true;
    CRYPT_DATA_BLOB pfx_blob;
    pfx_blob.cbData = static_cast<DWORD>(config.certificate.chain.size());
    pfx_blob.pbData = const_cast<BYTE*>(config.certificate.chain.data());
    std::wstring wide_pass = utf8_to_wide(config.certificate.password);
    m_cert_store = PFXImportCertStore(&pfx_blob, wide_pass.c_str(), CRYPT_EXPORTABLE | CRYPT_USER_KEYSET);
    const DWORD pfx_error = m_cert_store ? 0 : GetLastError();

    if (!m_cert_store) {
        if (!config.ca_store_path.empty()) {
            std::wstring wide_store = utf8_to_wide(config.ca_store_path);
            m_cert_store = CertOpenSystemStoreW(0, wide_store.c_str());
        } else {
            m_cert_store = CertOpenSystemStoreW(0, L"MY");
        }
    }

    if (!m_cert_store) {
        m_last_error = "PFXImportCertStore failed 0x" + hex32(static_cast<unsigned long>(pfx_error)) + " (wrong password or unsupported PKCS#12 encryption)";
        return false;
    }

    PCCERT_CONTEXT first = nullptr;
    PCCERT_CONTEXT it = nullptr;

    while ((it = CertEnumCertificatesInStore(m_cert_store, it)) != nullptr) {
        DWORD size = 0;
        const bool has_key = CertGetCertificateContextProperty(it, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &size)
            || CertGetCertificateContextProperty(it, CERT_NCRYPT_KEY_HANDLE_PROP_ID, nullptr, &size)
            || CertGetCertificateContextProperty(it, CERT_KEY_CONTEXT_PROP_ID, nullptr, &size);

        if (has_key) {
            m_cert_ctx = CertDuplicateCertificateContext(it);
            CertFreeCertificateContext(it);
            break;
        }

        if (!first) first = CertDuplicateCertificateContext(it);
    }

    if (m_cert_ctx) {
        if (first) CertFreeCertificateContext(first);
        return true;
    }

    m_cert_ctx = first;
    if (!m_cert_ctx) m_last_error = "No certificate found in the certificate store";
    return m_cert_ctx != nullptr;
}
#endif

#if defined(OS_WINDOWS)
SchannelSession::SchannelSession() noexcept {
    std::memset(&m_ctx, 0, sizeof(m_ctx));
    std::memset(&m_sizes, 0, sizeof(m_sizes));
}
#endif

#if defined(OS_WINDOWS)
SchannelSession::~SchannelSession() noexcept { close_session(); }
#endif

#if defined(OS_WINDOWS)
bool SchannelSession::open(NativeCredentialsBase& credentials, bool server, const std::string& hostname) noexcept {
    if (!credentials.acquired()) return false;
    close_session();
    m_cred     = static_cast<CredHandle*>(credentials.native());
    m_creds    = static_cast<const SchannelCredentials*>(&credentials);
    m_server   = server;
    m_hostname = hostname;
    m_alert    = AlertInfo();
    m_pending.clear();
    m_alpn_buffer.clear();
    const auto& alpn = m_creds->config().alpn_protocols;

    if (!alpn.empty()) {
        std::vector<std::uint8_t> wire;

        for (const auto& p : alpn) {
            if (p.empty() || p.size() > 255) return false;
            wire.push_back(static_cast<std::uint8_t>(p.size()));
            wire.insert(wire.end(), p.begin(), p.end());
        }

        const unsigned long list_size = static_cast<unsigned long>(sizeof(unsigned long) + sizeof(unsigned short) + wire.size());
        m_alpn_buffer.resize(sizeof(unsigned long) + list_size);
        std::uint8_t* p = m_alpn_buffer.data();
        const unsigned long ext = SecApplicationProtocolNegotiationExt_ALPN;
        const unsigned short wsize = static_cast<unsigned short>(wire.size());
        std::memcpy(p, &list_size, sizeof(list_size)); p += sizeof(list_size);
        std::memcpy(p, &ext, sizeof(ext)); p += sizeof(ext);
        std::memcpy(p, &wsize, sizeof(wsize)); p += sizeof(wsize);
        std::memcpy(p, wire.data(), wire.size());
    }

    return true;
}
#endif

#if defined(OS_WINDOWS)
void SchannelSession::close_session() noexcept {
    if (m_established) {
        DeleteSecurityContext(&m_ctx);
        std::memset(&m_ctx, 0, sizeof(m_ctx));
        m_established = false;
    }

    m_handshake_done = false;
    m_sizes_queried  = false;
    std::memset(&m_sizes, 0, sizeof(m_sizes));
    m_pending.clear();
}
#endif

#if defined(OS_WINDOWS)
bool SchannelSession::established() const noexcept { return m_handshake_done; }
#endif

#if defined(OS_WINDOWS)
auto SchannelSession::handshake(const std::uint8_t* input, std::size_t length) noexcept -> HandshakeIO {
    HandshakeIO io;

    if (!m_cred) {
        io.status = HandshakeStatus::Failed;
        io.detail = "No credentials";
        return io;
    }

    if (input && length > 0) { m_pending.insert(m_pending.end(), input, input + length); }

    if (m_server && m_pending.empty()) {
        io.status = HandshakeStatus::NeedMoreData;
        return io;
    }

    SecBuffer in_bufs[3] = {};
    in_bufs[0].BufferType = SECBUFFER_TOKEN;
    in_bufs[0].pvBuffer   = m_pending.empty() ? nullptr : m_pending.data();
    in_bufs[0].cbBuffer   = static_cast<ULONG>(m_pending.size());
    in_bufs[1].BufferType = SECBUFFER_EMPTY;
    SecBufferDesc in_desc = {};
    in_desc.ulVersion = SECBUFFER_VERSION;
    in_desc.cBuffers  = 2;
    in_desc.pBuffers  = in_bufs;
    const bool send_alpn = !m_alpn_buffer.empty() && !m_established;

    if (send_alpn) {
        in_bufs[2].BufferType = SECBUFFER_APPLICATION_PROTOCOLS;
        in_bufs[2].pvBuffer   = m_alpn_buffer.data();
        in_bufs[2].cbBuffer   = static_cast<ULONG>(m_alpn_buffer.size());
        in_desc.cBuffers      = 3;
    }

    SecBuffer alpn_only[1] = {};
    SecBufferDesc alpn_desc = {};

    if (send_alpn && !m_server && m_pending.empty()) {
        alpn_only[0] = in_bufs[2];
        alpn_desc.ulVersion = SECBUFFER_VERSION;
        alpn_desc.cBuffers  = 1;
        alpn_desc.pBuffers  = alpn_only;
    }
    SecBuffer out_buf = {};
    out_buf.BufferType = SECBUFFER_TOKEN;
    SecBufferDesc out_desc = {};
    out_desc.ulVersion = SECBUFFER_VERSION;
    out_desc.cBuffers  = 1;
    out_desc.pBuffers  = &out_buf;
    DWORD     out_flags = 0;
    TimeStamp expiry    = {};
    SECURITY_STATUS status;

    if (m_server) {
        status = AcceptSecurityContext(
            m_cred,
            m_established ? &m_ctx : nullptr,
            &in_desc,
            server_flags(),
            0,
            m_established ? nullptr : &m_ctx,
            &out_desc,
            &out_flags,
            &expiry
        );
    } else {
        std::wstring wide_host = utf8_to_wide(m_hostname);
        SEC_WCHAR* target = wide_host.empty() ? nullptr : const_cast<SEC_WCHAR*>(wide_host.c_str());

        status = InitializeSecurityContextW(
            m_cred,
            m_established ? &m_ctx : nullptr,
            target,
            client_flags(),
            0,
            0,
            m_pending.empty() ? (alpn_desc.cBuffers ? &alpn_desc : nullptr) : &in_desc,
            0,
            m_established ? nullptr : &m_ctx,
            &out_desc,
            &out_flags,
            &expiry
        );

        if (status == SEC_I_INCOMPLETE_CREDENTIALS) {
            if (out_buf.pvBuffer) { FreeContextBuffer(out_buf.pvBuffer); out_buf.pvBuffer = nullptr; }
            out_buf.cbBuffer = 0;
            out_buf.BufferType = SECBUFFER_TOKEN;
            in_bufs[0].BufferType = SECBUFFER_TOKEN;
            in_bufs[0].pvBuffer   = m_pending.empty() ? nullptr : m_pending.data();
            in_bufs[0].cbBuffer   = static_cast<ULONG>(m_pending.size());
            in_bufs[1].BufferType = SECBUFFER_EMPTY;
            in_bufs[1].pvBuffer   = nullptr;
            in_bufs[1].cbBuffer   = 0;
            in_desc.cBuffers      = 2;

            status = InitializeSecurityContextW(
                m_cred, &m_ctx, target, client_flags(), 0, 0, &in_desc, 0, nullptr, &out_desc, &out_flags, &expiry
            );
        }
    }

    if (out_buf.cbBuffer > 0 && out_buf.pvBuffer) {
        auto* p = static_cast<std::uint8_t*>(out_buf.pvBuffer);
        io.outgoing.assign(p, p + out_buf.cbBuffer);
    }

    if (out_buf.pvBuffer) { FreeContextBuffer(out_buf.pvBuffer); out_buf.pvBuffer = nullptr; }
    std::size_t extra = 0;

    if (in_bufs[1].BufferType == SECBUFFER_EXTRA && in_bufs[1].cbBuffer > 0) {
        extra = std::min<std::size_t>(in_bufs[1].cbBuffer, m_pending.size());
    }

    io.native_code = static_cast<int>(status);

    switch (status) {
        case SEC_E_OK:
            m_established    = true;
            m_handshake_done = true;
            io.status = HandshakeStatus::Complete;
            take_extra(extra, io.leftover);
            m_pending.clear();
            return io;

        case SEC_I_CONTINUE_NEEDED:
            m_established = true;
            io.status = HandshakeStatus::Continue;
            take_extra(extra, io.leftover);
            m_pending.assign(io.leftover.begin(), io.leftover.end());
            io.leftover.clear();
            return io;

        case SEC_E_INCOMPLETE_MESSAGE:
            io.status = HandshakeStatus::NeedMoreData;
            return io;   

        case SEC_I_COMPLETE_NEEDED:
        case SEC_I_COMPLETE_AND_CONTINUE: {
            CompleteAuthToken(&m_ctx, &out_desc);
            m_established = true;

            if (status == SEC_I_COMPLETE_AND_CONTINUE) {
                io.status = HandshakeStatus::Continue;
                take_extra(extra, io.leftover);
                m_pending.assign(io.leftover.begin(), io.leftover.end());
                io.leftover.clear();
            } else {
                m_handshake_done = true;
                io.status = HandshakeStatus::Complete;
                take_extra(extra, io.leftover);
                m_pending.clear();
            }

            return io;
        }

        case SEC_E_CERT_EXPIRED:
        case SEC_E_CERT_UNKNOWN:
        case SEC_E_UNTRUSTED_ROOT:
            note_failure(status);
            io.status = HandshakeStatus::Failed;
            io.detail = "Certificate validation failed (0x" + hex32(static_cast<unsigned long>(status)) + ")";
            return io;

        case SEC_E_WRONG_PRINCIPAL:
            note_failure(status);
            io.status = HandshakeStatus::Failed;
            io.detail = "Server name mismatch";
            return io;

        case SEC_E_ALGORITHM_MISMATCH:
            note_failure(status);
            io.status = HandshakeStatus::Failed;
            io.detail = "No common cipher suite";
            return io;

        case SEC_E_NO_CREDENTIALS:
        case SEC_E_INCOMPLETE_CREDENTIALS:
            note_failure(status);
            io.status = HandshakeStatus::Failed;
            io.detail = "Credentials missing or incomplete";
            return io;

        default:
            note_failure(status);
            io.status = HandshakeStatus::Failed;
            io.detail = "Schannel error 0x" + hex32(static_cast<unsigned long>(status));
            return io;
    }
}
#endif

#if defined(OS_WINDOWS)
int SchannelSession::encrypt(const std::uint8_t* plaintext, std::size_t length, std::vector<std::uint8_t>& out) noexcept {
    if (!m_handshake_done || !plaintext || length == 0) return -1;
    if (!ensure_sizes()) return -1;
    std::size_t sent = 0;

    while (sent < length) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(length - sent, m_sizes.cbMaximumMessage));
        std::vector<std::uint8_t> msg(m_sizes.cbHeader + chunk + m_sizes.cbTrailer);
        std::memcpy(msg.data() + m_sizes.cbHeader, plaintext + sent, chunk);
        SecBuffer bufs[4] = {};
        bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
        bufs[0].cbBuffer   = m_sizes.cbHeader;
        bufs[0].pvBuffer   = msg.data();
        bufs[1].BufferType = SECBUFFER_DATA;
        bufs[1].cbBuffer   = chunk;
        bufs[1].pvBuffer   = msg.data() + m_sizes.cbHeader;
        bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
        bufs[2].cbBuffer   = m_sizes.cbTrailer;
        bufs[2].pvBuffer   = msg.data() + m_sizes.cbHeader + chunk;
        bufs[3].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc desc = {};
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers  = 4;
        desc.pBuffers  = bufs;
        SECURITY_STATUS status = EncryptMessage(&m_ctx, 0, &desc, 0);
        if (FAILED(status)) { return sent > 0 ? static_cast<int>(sent) : -1; }
        const std::size_t wire = bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer;
        out.insert(out.end(), msg.begin(), msg.begin() + static_cast<std::ptrdiff_t>(wire));
        sent += chunk;
    }

    return static_cast<int>(sent);
}
#endif

#if defined(OS_WINDOWS)
auto SchannelSession::decrypt(const std::uint8_t* ciphertext, std::size_t length) noexcept -> DecryptIO {
    DecryptIO io;
    if (!m_handshake_done) { io.status = IOStatus::Failed; return io; }
    if (!ensure_sizes())   { io.status = IOStatus::Failed; return io; }
    if (!ciphertext || length == 0) { io.status = IOStatus::NeedMoreData; return io; }
    std::vector<std::uint8_t> work(ciphertext, ciphertext + length);
    SecBuffer bufs[4] = {};
    bufs[0].BufferType = SECBUFFER_DATA;
    bufs[0].cbBuffer   = static_cast<ULONG>(work.size());
    bufs[0].pvBuffer   = work.data();
    bufs[1].BufferType = SECBUFFER_EMPTY;
    bufs[2].BufferType = SECBUFFER_EMPTY;
    bufs[3].BufferType = SECBUFFER_EMPTY;
    SecBufferDesc desc = {};
    desc.ulVersion = SECBUFFER_VERSION;
    desc.cBuffers  = 4;
    desc.pBuffers  = bufs;
    SECURITY_STATUS status = DecryptMessage(&m_ctx, &desc, 0, nullptr);
    io.native_code = static_cast<int>(status);

    if (status == SEC_E_INCOMPLETE_MESSAGE) {
        io.status   = IOStatus::NeedMoreData;
        io.leftover = std::move(work);
        return io;
    }

    collect_extra(bufs, work, io.leftover);

    if (status == SEC_I_CONTEXT_EXPIRED) {
        m_alert.present = true;
        m_alert.received = true;
        m_alert.level = 1;
        m_alert.description = 0;
        io.status = IOStatus::Closed;
        return io;
    }

    if (status == SEC_I_RENEGOTIATE) {
        io.status = IOStatus::Renegotiate;
        m_handshake_done = false;   
        m_pending.assign(io.leftover.begin(), io.leftover.end());
        io.leftover.clear();
        return io;
    }

    if (FAILED(status)) { note_failure(status); io.status = IOStatus::Failed; return io; }

    for (int i = 0; i < 4; ++i) {
        if (bufs[i].BufferType == SECBUFFER_DATA && bufs[i].cbBuffer > 0 && bufs[i].pvBuffer) {
            auto* p = static_cast<std::uint8_t*>(bufs[i].pvBuffer);
            io.plaintext.assign(p, p + bufs[i].cbBuffer);
            break;
        }
    }

    io.status = IOStatus::Ok;
    return io;
}
#endif

#if defined(OS_WINDOWS)
bool SchannelSession::close_notify(std::vector<std::uint8_t>& out) noexcept {
    if (!m_established || !m_cred) return false;
    DWORD token = SCHANNEL_SHUTDOWN;
    SecBuffer token_buf = {};
    token_buf.BufferType = SECBUFFER_TOKEN;
    token_buf.cbBuffer   = sizeof(token);
    token_buf.pvBuffer   = &token;
    SecBufferDesc token_desc = {};
    token_desc.ulVersion = SECBUFFER_VERSION;
    token_desc.cBuffers  = 1;
    token_desc.pBuffers  = &token_buf;
    if (FAILED(ApplyControlToken(&m_ctx, &token_desc))) return false;
    SecBuffer out_buf = {};
    out_buf.BufferType = SECBUFFER_TOKEN;
    SecBufferDesc out_desc = {};
    out_desc.ulVersion = SECBUFFER_VERSION;
    out_desc.cBuffers  = 1;
    out_desc.pBuffers  = &out_buf;
    DWORD     out_flags = 0;
    TimeStamp expiry    = {};

    if (m_server) {
        AcceptSecurityContext(m_cred, &m_ctx, nullptr, server_flags(), 0, nullptr, &out_desc, &out_flags, &expiry);
    } else {
        InitializeSecurityContextW(m_cred, &m_ctx, nullptr, client_flags(), 0, 0, nullptr, 0, nullptr, &out_desc, &out_flags, &expiry);
    }

    if (out_buf.cbBuffer > 0 && out_buf.pvBuffer) {
        auto* p = static_cast<std::uint8_t*>(out_buf.pvBuffer);
        out.assign(p, p + out_buf.cbBuffer);
        FreeContextBuffer(out_buf.pvBuffer);
        return true;
    }

    return false;
}
#endif

#if defined(OS_WINDOWS)
bool SchannelSession::peer_chain(std::vector<std::vector<std::uint8_t>>& out) const noexcept {
    if (!m_established) return false;
    PCCERT_CONTEXT peer = nullptr;

    SECURITY_STATUS status = QueryContextAttributes(
        const_cast<PCtxtHandle>(&m_ctx),
        SECPKG_ATTR_REMOTE_CERT_CONTEXT,
        &peer
    );

    if (FAILED(status) || !peer) return false;
    out.emplace_back(peer->pbCertEncoded, peer->pbCertEncoded + peer->cbCertEncoded);

    if (peer->hCertStore) {
        PCCERT_CONTEXT c = nullptr;

        while ((c = CertEnumCertificatesInStore(peer->hCertStore, c)) != nullptr) {
            if (c->cbCertEncoded == peer->cbCertEncoded &&
                std::memcmp(c->pbCertEncoded, peer->pbCertEncoded, c->cbCertEncoded) == 0) {
                continue;
            }
                
            out.emplace_back(c->pbCertEncoded, c->pbCertEncoded + c->cbCertEncoded);
        }
    }

    CertFreeCertificateContext(peer);
    return true;
}
#endif

#if defined(OS_WINDOWS)
std::string SchannelSession::cipher_name() const noexcept {
    if (!m_established) return {};
    SecPkgContext_CipherInfo info = {};
    info.dwVersion = SECPKGCONTEXT_CIPHERINFO_V1;

    if (FAILED(QueryContextAttributes(const_cast<PCtxtHandle>(&m_ctx), SECPKG_ATTR_CIPHER_INFO, &info))) {
        return {};
    }

    return wide_to_utf8(info.szCipherSuite);
}
#endif

#if defined(OS_WINDOWS)
auto SchannelSession::protocol_version() const noexcept -> TLSVersion {
    if (!m_established) return TLSVersion::TLS_1_2;
    SecPkgContext_ConnectionInfo conn = {};

    if (FAILED(QueryContextAttributes(const_cast<PCtxtHandle>(&m_ctx), SECPKG_ATTR_CONNECTION_INFO, &conn))) {
        return TLSVersion::TLS_1_2;
    }

    switch (conn.dwProtocol) {
        case SP_PROT_TLS1_0_CLIENT: case SP_PROT_TLS1_0_SERVER: return TLSVersion::TLS_1_0;
        case SP_PROT_TLS1_1_CLIENT: case SP_PROT_TLS1_1_SERVER: return TLSVersion::TLS_1_1;
        case SP_PROT_TLS1_2_CLIENT: case SP_PROT_TLS1_2_SERVER: return TLSVersion::TLS_1_2;
    #ifdef SP_PROT_TLS1_3
        case SP_PROT_TLS1_3_CLIENT: case SP_PROT_TLS1_3_SERVER: return TLSVersion::TLS_1_3;
    #endif
        default: return TLSVersion::TLS_1_2;
    }
}
#endif

#if defined(OS_WINDOWS)
std::size_t SchannelSession::max_plaintext_chunk() const noexcept {
    return m_sizes_queried ? m_sizes.cbMaximumMessage : 16384;
}
#endif

#if defined(OS_WINDOWS)
auto SchannelSession::client_flags() noexcept -> DWORD {
    return ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT
         | ISC_REQ_CONFIDENTIALITY | ISC_REQ_STREAM
         | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_MANUAL_CRED_VALIDATION;
}
#endif

#if defined(OS_WINDOWS)
auto SchannelSession::server_flags() const noexcept -> DWORD {
    DWORD f = ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT
            | ASC_REQ_CONFIDENTIALITY | ASC_REQ_STREAM
            | ASC_REQ_ALLOCATE_MEMORY;
    if (m_creds && m_creds->config().verify_mode != VerifyMode::None) f |= ASC_REQ_MUTUAL_AUTH;
    return f;
}
#endif

#if defined(OS_WINDOWS)
bool SchannelSession::ensure_sizes() noexcept {
    if (m_sizes_queried) return true;
    if (!m_established) return false;
    std::memset(&m_sizes, 0, sizeof(m_sizes));
    SECURITY_STATUS status = QueryContextAttributes(&m_ctx, SECPKG_ATTR_STREAM_SIZES, &m_sizes);
    m_sizes_queried = SUCCEEDED(status);
    return m_sizes_queried;
}
#endif

#if defined(OS_WINDOWS)
void SchannelSession::take_extra(std::size_t extra, std::vector<std::uint8_t>& out) noexcept {
    if (extra == 0 || extra > m_pending.size()) return;
    out.assign(m_pending.end() - static_cast<std::ptrdiff_t>(extra), m_pending.end());
}
#endif

#if defined(OS_WINDOWS)
void SchannelSession::collect_extra(
    SecBuffer bufs[4],
    const std::vector<std::uint8_t>& work,
    std::vector<std::uint8_t>& out
) noexcept {
    for (int i = 0; i < 4; ++i) {
        if (bufs[i].BufferType == SECBUFFER_EXTRA && bufs[i].cbBuffer > 0) {
            std::size_t extra = std::min<std::size_t>(bufs[i].cbBuffer, work.size());
            out.assign(work.end() - static_cast<std::ptrdiff_t>(extra), work.end());
            return;
        }
    }
}
#endif

#if defined(OS_WINDOWS)
std::uint8_t SchannelSession::status_to_alert(SECURITY_STATUS status) noexcept {
    switch (static_cast<unsigned long>(status)) {
        case static_cast<unsigned long>(SEC_E_CERT_EXPIRED):         return 45;
        case static_cast<unsigned long>(SEC_E_UNTRUSTED_ROOT):       return 48;
        case static_cast<unsigned long>(SEC_E_CERT_UNKNOWN):         return 46;
        case static_cast<unsigned long>(SEC_E_WRONG_PRINCIPAL):      return 42;
        case static_cast<unsigned long>(SEC_E_ALGORITHM_MISMATCH):   return 40;
        case static_cast<unsigned long>(SEC_E_UNSUPPORTED_FUNCTION): return 70;
        case static_cast<unsigned long>(SEC_E_ILLEGAL_MESSAGE):      return 10;
        case static_cast<unsigned long>(SEC_E_DECRYPT_FAILURE):      return 20;
        case static_cast<unsigned long>(SEC_E_MESSAGE_ALTERED):      return 20;
        case static_cast<unsigned long>(SEC_E_NO_CREDENTIALS):       return 116;
        case static_cast<unsigned long>(SEC_E_INCOMPLETE_CREDENTIALS): return 116;
        case 0x80090367ul:                                           return 120;
        default:                                                     return 80;
    }
}

void SchannelSession::note_failure(SECURITY_STATUS status) noexcept {
    m_alert.present = true;
    m_alert.received = false;
    m_alert.level = 2;
    m_alert.description = status_to_alert(status);
}

std::string SchannelSession::alpn_selected() const noexcept {
    if (!m_handshake_done || m_alpn_buffer.empty()) return {};
    SecPkgContext_ApplicationProtocol proto;
    std::memset(&proto, 0, sizeof(proto));
    if (FAILED(QueryContextAttributes(const_cast<PCtxtHandle>(&m_ctx), SECPKG_ATTR_APPLICATION_PROTOCOL, &proto))) return {};
    if (proto.ProtoNegoStatus != SecApplicationProtocolNegotiationStatus_Success) return {};
    return std::string(reinterpret_cast<const char*>(proto.ProtocolId), proto.ProtocolIdSize);
}

bool SchannelSession::session_reused() const noexcept {
    if (!m_handshake_done) return false;
    SecPkgContext_SessionInfo info;
    std::memset(&info, 0, sizeof(info));
    if (FAILED(QueryContextAttributes(const_cast<PCtxtHandle>(&m_ctx), SECPKG_ATTR_SESSION_INFO, &info))) return false;
    return (info.dwFlags & SSL_SESSION_RECONNECT) != 0;
}

bool SchannelSession::export_keying_material(const std::string& label, const std::uint8_t* context, std::size_t context_len,
                                             bool use_context, std::size_t out_len, std::vector<std::uint8_t>& out) const noexcept {
    if (!m_handshake_done || out_len == 0) return false;
    SecPkgContext_KeyingMaterialInfo info;
    std::memset(&info, 0, sizeof(info));
    std::string l = label;
    info.cbLabel = static_cast<WORD>(l.size() + 1);
    info.pszLabel = &l[0];
    std::vector<std::uint8_t> ctx(context ? context : nullptr, context ? context + context_len : nullptr);
    info.cbContextValue = use_context ? static_cast<WORD>(ctx.size()) : 0;
    info.pbContextValue = (use_context && !ctx.empty()) ? ctx.data() : nullptr;
    info.cbKeyingMaterial = static_cast<DWORD>(out_len);
    if (FAILED(SetContextAttributesW(const_cast<PCtxtHandle>(&m_ctx), SECPKG_ATTR_KEYING_MATERIAL_INFO, &info, sizeof(info)))) return false;
    SecPkgContext_KeyingMaterial km;
    std::memset(&km, 0, sizeof(km));
    if (FAILED(QueryContextAttributes(const_cast<PCtxtHandle>(&m_ctx), SECPKG_ATTR_KEYING_MATERIAL, &km))) return false;
    if (km.pbKeyingMaterial && km.cbKeyingMaterial == out_len) out.assign(km.pbKeyingMaterial, km.pbKeyingMaterial + km.cbKeyingMaterial);
    if (km.pbKeyingMaterial) { SecureZeroMemory(km.pbKeyingMaterial, km.cbKeyingMaterial); FreeContextBuffer(km.pbKeyingMaterial); }
    return out.size() == out_len;
}

bool SchannelSession::channel_binding(ChannelBindingType type, std::vector<std::uint8_t>& out) const noexcept {
    if (!m_handshake_done) return false;
    out.clear();

    if (type == ChannelBindingType::TlsExporter) {
        if (protocol_version() != TLSVersion::TLS_1_3) return false;
        return export_keying_material("EXPORTER-Channel-Binding", nullptr, 0, true, 32, out);
    }

    const unsigned long attr = type == ChannelBindingType::TlsUnique ? SECPKG_ATTR_UNIQUE_BINDINGS : SECPKG_ATTR_ENDPOINT_BINDINGS;
    SecPkgContext_Bindings b;
    std::memset(&b, 0, sizeof(b));
    if (FAILED(QueryContextAttributes(const_cast<PCtxtHandle>(&m_ctx), attr, &b)) || !b.Bindings) return false;
    const auto* base = reinterpret_cast<const std::uint8_t*>(b.Bindings);
    const std::uint8_t* data = base + b.Bindings->dwApplicationDataOffset;
    std::size_t len = b.Bindings->cbApplicationDataLength;
    const char* prefix = type == ChannelBindingType::TlsUnique ? "tls-unique:" : "tls-server-end-point:";
    const std::size_t plen = std::strlen(prefix);

    if (len >= plen && std::memcmp(data, prefix, plen) == 0) {
        data += plen;
        len -= plen;
    }

    out.assign(data, data + len);
    FreeContextBuffer(b.Bindings);
    return !out.empty();
}

bool SchannelSession::local_certificate(std::vector<std::uint8_t>& out) const noexcept {
    if (!m_established) return false;
    PCCERT_CONTEXT local = nullptr;
    if (FAILED(QueryContextAttributes(const_cast<PCtxtHandle>(&m_ctx), SECPKG_ATTR_LOCAL_CERT_CONTEXT, &local)) || !local) return false;
    out.assign(local->pbCertEncoded, local->pbCertEncoded + local->cbCertEncoded);
    CertFreeCertificateContext(local);
    return true;
}

OCSPResult SchannelCertOps::verify_ocsp(
    const std::vector<std::uint8_t>&,
    const NativeCertificateBase&,
    const NativeCertificateBase*,
    const std::string&
) noexcept {
    OCSPResult r;
    r.status = OCSPCertStatus::Unsupported;
    r.detail = "Schannel validates stapled OCSP responses internally when revocation checking is enabled";
    return r;
}

bool SchannelCertOps::load_system_roots(std::vector<std::vector<std::uint8_t>>& out, const std::string& store_name) noexcept {
    std::wstring name = utf8_to_wide(store_name.empty() ? std::string("ROOT") : store_name);
    HCERTSTORE store = CertOpenSystemStoreW(0, name.c_str());
    if (!store) return false;
    PCCERT_CONTEXT c = nullptr;
    const std::size_t before = out.size();
    while ((c = CertEnumCertificatesInStore(store, c)) != nullptr) out.emplace_back(c->pbCertEncoded, c->pbCertEncoded + c->cbCertEncoded);
    CertCloseStore(store, 0);
    return out.size() > before;
}

void SchannelPrivateKey::release() noexcept {
    if (m_key) { NCryptFreeObject(m_key); m_key = 0; }
    if (m_prov) { NCryptFreeObject(m_prov); m_prov = 0; }
}

bool SchannelPrivateKey::import_pkcs8(const std::uint8_t* der, std::size_t len, const std::string& passphrase) noexcept {
    release();

    if (FAILED(NCryptOpenStorageProvider(&m_prov, MS_KEY_STORAGE_PROVIDER, 0))) {
        m_last_error = "NCryptOpenStorageProvider failed";
        return false;
    }

    std::wstring wpass = utf8_to_wide(passphrase);
    NCryptBuffer buffer;
    buffer.cbBuffer = static_cast<ULONG>((wpass.size() + 1) * sizeof(wchar_t));
    buffer.BufferType = NCRYPTBUFFER_PKCS_SECRET;
    buffer.pvBuffer = &wpass[0];
    NCryptBufferDesc params;
    params.ulVersion = NCRYPTBUFFER_VERSION;
    params.cBuffers = 1;
    params.pBuffers = &buffer;

    SECURITY_STATUS st = NCryptImportKey(m_prov, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, passphrase.empty() ? nullptr : &params,
                                         &m_key, const_cast<PBYTE>(der), static_cast<DWORD>(len), NCRYPT_DO_NOT_FINALIZE_FLAG);

    if (FAILED(st)) {
        m_key = 0;
        m_last_error = "NCryptImportKey failed 0x" + hex32(static_cast<unsigned long>(st));
        return false;
    }

    DWORD policy = NCRYPT_ALLOW_EXPORT_FLAG | NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG;
    NCryptSetProperty(m_key, NCRYPT_EXPORT_POLICY_PROPERTY, reinterpret_cast<PBYTE>(&policy), sizeof(policy), 0);
    st = NCryptFinalizeKey(m_key, 0);

    if (FAILED(st)) {
        m_last_error = "NCryptFinalizeKey failed 0x" + hex32(static_cast<unsigned long>(st));
        release();
        return false;
    }

    return true;
}

bool SchannelPrivateKey::load_pem(const std::string& pem, const std::string& passphrase) noexcept {
    try {
        for (const auto& block : PEM::decode_all(pem)) {
            if (block.label.find("PRIVATE KEY") == std::string::npos) continue;

            if (block.encrypted) {
                m_last_error = "Legacy encrypted PEM keys are not supported by the Schannel backend; convert to encrypted PKCS#8";
                return false;
            }

            if (block.label == "PRIVATE KEY" || block.label == "ENCRYPTED PRIVATE KEY") {
                return import_pkcs8(block.der.data(), block.der.size(), passphrase);
            }

            std::vector<std::uint8_t> wrapped;
            if (!der::wrap_traditional_key(block.label, block.der, wrapped)) {
                m_last_error = "Unsupported private key type \"" + block.label + "\"";
                return false;
            }

            return import_pkcs8(wrapped.data(), wrapped.size(), passphrase);
        }
    } catch (...) {
        m_last_error = "Out of memory";
        return false;
    }

    m_last_error = "No private key found in PEM data";
    return false;
}

bool SchannelPrivateKey::load_der(const std::uint8_t* data, std::size_t len, const std::string& passphrase) noexcept {
    if (import_pkcs8(data, len, passphrase)) return true;
    std::vector<std::uint8_t> raw(data, data + len), wrapped;
    if (der::wrap_traditional_key("RSA PRIVATE KEY", raw, wrapped) && import_pkcs8(wrapped.data(), wrapped.size(), passphrase)) return true;
    if (der::wrap_traditional_key("EC PRIVATE KEY", raw, wrapped) && import_pkcs8(wrapped.data(), wrapped.size(), passphrase)) return true;
    return false;
}

KeyType SchannelPrivateKey::key_type() const noexcept {
    if (!m_key) return KeyType::None;
    wchar_t group[64] = {};
    DWORD got = 0;
    if (FAILED(NCryptGetProperty(m_key, NCRYPT_ALGORITHM_GROUP_PROPERTY, reinterpret_cast<PBYTE>(group), sizeof(group), &got, 0))) return KeyType::Other;
    const std::wstring g(group);
    if (g == L"RSA") return KeyType::RSA;
    if (g == L"ECDSA" || g == L"ECDH") return KeyType::EC;
    if (g == L"DSA") return KeyType::DSA;
    return KeyType::Other;
}

int SchannelPrivateKey::key_bits() const noexcept {
    if (!m_key) return 0;
    DWORD bits = 0, got = 0;
    if (FAILED(NCryptGetProperty(m_key, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<PBYTE>(&bits), sizeof(bits), &got, 0))) return 0;
    return static_cast<int>(bits);
}

bool SchannelPrivateKey::export_public(std::vector<std::uint8_t>& out) const noexcept {
    DWORD size = 0;
    if (FAILED(NCryptExportKey(m_key, 0, BCRYPT_PUBLIC_KEY_BLOB, nullptr, nullptr, 0, &size, 0)) || size == 0) return false;
    out.resize(size);
    if (FAILED(NCryptExportKey(m_key, 0, BCRYPT_PUBLIC_KEY_BLOB, nullptr, out.data(), size, &size, 0))) return false;
    out.resize(size);
    return true;
}

bool SchannelPrivateKey::matches_certificate(const NativeCertificateBase& cert) const noexcept {
    auto ctx = static_cast<PCCERT_CONTEXT>(cert.native());
    if (!m_key || !ctx) return false;
    std::vector<std::uint8_t> mine;
    if (!export_public(mine)) return false;
    BCRYPT_KEY_HANDLE bkey = nullptr;
    if (!CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, &ctx->pCertInfo->SubjectPublicKeyInfo, 0, nullptr, &bkey) || !bkey) return false;
    ULONG size = 0;
    std::vector<std::uint8_t> theirs;

    if (BCryptExportKey(bkey, nullptr, BCRYPT_PUBLIC_KEY_BLOB, nullptr, 0, &size, 0) == 0 && size > 0) {
        theirs.resize(size);
        if (BCryptExportKey(bkey, nullptr, BCRYPT_PUBLIC_KEY_BLOB, theirs.data(), size, &size, 0) != 0) theirs.clear();
    }

    BCryptDestroyKey(bkey);
    if (theirs.size() < 8 || mine.size() != theirs.size()) return false;
    return std::memcmp(mine.data() + 4, theirs.data() + 4, mine.size() - 4) == 0;
}

bool SchannelPrivateKey::to_pkcs8_der(std::vector<std::uint8_t>& out) const noexcept {
    if (!m_key) return false;
    DWORD size = 0;
    if (FAILED(NCryptExportKey(m_key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, nullptr, nullptr, 0, &size, 0)) || size == 0) return false;
    out.resize(size);
    if (FAILED(NCryptExportKey(m_key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, nullptr, out.data(), size, &size, 0))) { out.clear(); return false; }
    out.resize(size);
    return true;
}

bool SchannelPrivateKey::build_pkcs12(const NativeCertificateBase& leaf, const std::vector<const NativeCertificateBase*>& chain,
                                      const std::string& password, std::vector<std::uint8_t>& out) const noexcept {
    auto leaf_ctx = static_cast<PCCERT_CONTEXT>(leaf.native());
    if (!m_key || !leaf_ctx) return false;
    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, nullptr);
    if (!store) return false;
    PCCERT_CONTEXT added = nullptr;

    if (!CertAddCertificateContextToStore(store, leaf_ctx, CERT_STORE_ADD_ALWAYS, &added) || !added) {
        CertCloseStore(store, 0);
        return false;
    }

    NCRYPT_KEY_HANDLE handle = m_key;
    const BOOL linked = CertSetCertificateContextProperty(added, CERT_NCRYPT_KEY_HANDLE_PROP_ID, CERT_SET_PROPERTY_INHIBIT_PERSIST_FLAG, &handle);

    for (const auto* c : chain) {
        auto cc = c ? static_cast<PCCERT_CONTEXT>(c->native()) : nullptr;
        if (cc) CertAddCertificateContextToStore(store, cc, CERT_STORE_ADD_USE_EXISTING, nullptr);
    }

    std::wstring wpass = utf8_to_wide(password);
    CRYPT_DATA_BLOB blob = { 0, nullptr };
    const DWORD flags = EXPORT_PRIVATE_KEYS | REPORT_NO_PRIVATE_KEY | REPORT_NOT_ABLE_TO_EXPORT_PRIVATE_KEY;
    bool ok = linked && PFXExportCertStoreEx(store, &blob, wpass.c_str(), nullptr, flags) && blob.cbData > 0;

    if (ok) {
        out.resize(blob.cbData);
        blob.pbData = out.data();
        ok = PFXExportCertStoreEx(store, &blob, wpass.c_str(), nullptr, flags) != FALSE;
        out.resize(ok ? blob.cbData : 0);
    }

    CertSetCertificateContextProperty(added, CERT_NCRYPT_KEY_HANDLE_PROP_ID, 0, nullptr);
    CertFreeCertificateContext(added);
    CertCloseStore(store, 0);
    return ok;
}
#endif

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo
