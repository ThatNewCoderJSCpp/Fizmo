#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "schannel_backend.hpp"

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
    bool check_revocation
) noexcept -> VerifyResult {
    auto leaf_ctx = static_cast<PCCERT_CONTEXT>(leaf.native());
    if (!leaf_ctx) { return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "No leaf certificate"); }
    HCERTSTORE extra_store = build_extra_store(extras, ca_store_path);
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
        nullptr,
        leaf_ctx,
        nullptr,
        extra_store ? extra_store : leaf_ctx->hCertStore,
        &chain_params,
        chain_flags,
        nullptr,
        &chain_ctx
    );

    if (!ok || !chain_ctx) {
        if (extra_store) CertCloseStore(extra_store, 0);
        return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "CertGetCertificateChain failed");
    }

    VerifyResult result = inspect_chain_status(chain_ctx);

    if (!result.passed) {
        CertFreeCertificateChain(chain_ctx);
        if (extra_store) CertCloseStore(extra_store, 0);
        return result;
    }

    std::wstring wide_host = utf8_to_wide(hostname);
    SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_policy = {};
    ssl_policy.cbSize         = sizeof(ssl_policy);
    ssl_policy.dwAuthType     = AUTHTYPE_SERVER;
    ssl_policy.fdwChecks      = 0;
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

    CertFreeCertificateChain(chain_ctx);
    if (extra_store) CertCloseStore(extra_store, 0);

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
auto SchannelCertOps::build_extra_store(
    const std::vector<const NativeCertificateBase*>& extras,
    const std::string& ca_store_path
) noexcept -> HCERTSTORE {
    if (extras.empty() && ca_store_path.empty()) return nullptr;

    HCERTSTORE store = CertOpenStore(
        CERT_STORE_PROV_MEMORY, 0, 0,
        CERT_STORE_CREATE_NEW_FLAG, nullptr
    );

    if (!store) return nullptr;

    for (const auto* cert : extras) {
        if (!cert) continue;
        auto ctx = static_cast<PCCERT_CONTEXT>(cert->native());
        if (!ctx) continue;
        CertAddCertificateContextToStore(store, ctx, CERT_STORE_ADD_USE_EXISTING, nullptr);
    }

    if (!ca_store_path.empty()) {
        std::wstring wide = utf8_to_wide(ca_store_path);

        HCERTSTORE file_store = CertOpenStore(
            CERT_STORE_PROV_FILENAME_W, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            0, CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG,
            wide.c_str()
        );

        if (file_store) {
            PCCERT_CONTEXT c = nullptr;

            while ((c = CertEnumCertificatesInStore(file_store, c)) != nullptr) {
                CertAddCertificateContextToStore(store, c, CERT_STORE_ADD_USE_EXISTING, nullptr);
            }

            CertCloseStore(file_store, 0);
        }
    }

    return store;
}
#endif

#if defined(OS_WINDOWS)
auto SchannelCertOps::inspect_chain_status(PCCERT_CHAIN_CONTEXT chain_ctx) noexcept -> VerifyResult {
    const DWORD flags = chain_ctx->TrustStatus.dwErrorStatus;
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
bool SchannelCredentials::acquire(const TLSConfig& config) noexcept {
    release();

    if (!open_certificate_store(config)) {
        m_last_error = "Certificate store could not be opened";
        return false;
    }

    SCHANNEL_CRED cred;
    std::memset(&cred, 0, sizeof(cred));
    cred.dwVersion             = SCHANNEL_CRED_VERSION;
    cred.grbitEnabledProtocols = protocol_flags(config);

    if (m_cert_ctx) {
        cred.cCreds = 1;
        cred.paCred = &m_cert_ctx;
    }

    switch (config.verify_mode) {
        case VerifyMode::None:
            cred.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_SERVERNAME_CHECK;
            break;
        case VerifyMode::Peer:
        case VerifyMode::FailIfNoPeer:
            cred.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION;
            if (config.check_revocation) { cred.dwFlags |= SCH_CRED_REVOCATION_CHECK_CHAIN; }
            break;
    }

    const unsigned long use = (config.use == ContextUse::Server) ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND;
    TimeStamp expiry;

    SECURITY_STATUS status = AcquireCredentialsHandleW(
        nullptr,
        const_cast<SEC_WCHAR*>(UNISP_NAME_W),
        use,
        nullptr,
        &cred,
        nullptr,
        nullptr,
        &m_cred,
        &expiry
    );

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

    if (!m_cert_store) {
        if (!config.ca_store_path.empty()) {
            std::wstring wide_store = utf8_to_wide(config.ca_store_path);
            m_cert_store = CertOpenSystemStoreW(0, wide_store.c_str());
        } else {
            m_cert_store = CertOpenSystemStoreW(0, L"MY");
        }
    }

    if (!m_cert_store) return false;

    m_cert_ctx = CertFindCertificateInStore(
        m_cert_store,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_FIND_ANY,
        nullptr,
        nullptr
    );

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
    m_server   = server;
    m_hostname = hostname;
    m_pending.clear();
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

    SecBuffer in_bufs[2] = {};
    in_bufs[0].BufferType = SECBUFFER_TOKEN;
    in_bufs[0].pvBuffer   = m_pending.empty() ? nullptr : m_pending.data();
    in_bufs[0].cbBuffer   = static_cast<ULONG>(m_pending.size());
    in_bufs[1].BufferType = SECBUFFER_EMPTY;
    SecBufferDesc in_desc = {};
    in_desc.ulVersion = SECBUFFER_VERSION;
    in_desc.cBuffers  = 2;
    in_desc.pBuffers  = in_bufs;
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
            m_pending.empty() ? nullptr : &in_desc,
            0,
            m_established ? nullptr : &m_ctx,
            &out_desc,
            &out_flags,
            &expiry
        );
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
            io.status = HandshakeStatus::Failed;
            io.detail = "Certificate validation failed (0x" + hex32(static_cast<unsigned long>(status)) + ")";
            return io;

        case SEC_E_WRONG_PRINCIPAL:
            io.status = HandshakeStatus::Failed;
            io.detail = "Server name mismatch";
            return io;

        case SEC_E_ALGORITHM_MISMATCH:
            io.status = HandshakeStatus::Failed;
            io.detail = "No common cipher suite";
            return io;

        case SEC_E_NO_CREDENTIALS:
        case SEC_E_INCOMPLETE_CREDENTIALS:
            io.status = HandshakeStatus::Failed;
            io.detail = "Credentials missing or incomplete";
            return io;

        default:
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

    if (FAILED(status)) { io.status = IOStatus::Failed; return io; }

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
auto SchannelSession::server_flags() noexcept -> DWORD {
    return ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT
         | ASC_REQ_CONFIDENTIALITY | ASC_REQ_STREAM
         | ASC_REQ_ALLOCATE_MEMORY;
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

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo
