#ifndef FIZMO_TLS_CONTEXT_HPP
#define FIZMO_TLS_CONTEXT_HPP

#include "../../Basic/basic_includes.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <memory>

namespace fizmo {
namespace networking {
namespace security {

enum class TLSVersion : std::uint32_t {
    TLS_1_0 = 0x0301,
    TLS_1_1 = 0x0302,
    TLS_1_2 = 0x0303,
    TLS_1_3 = 0x0304
};

enum class VerifyMode {
    None = 0,         
    Peer,           // Verify the remote certificate; fail if invalid
    FailIfNoPeer    // Like Peer, but also fail if no certificate is presented 
};

class TLSContext {
public:
    struct CertificateData {
        std::vector<std::uint8_t> chain;            // DER / PFX blob
        std::vector<std::uint8_t> private_key;      // DER blob (empty when bundled in PFX)
        std::string               password;         // PFX passphrase (empty if none)
        bool empty() const noexcept { return chain.empty(); }
        void clear() noexcept { chain.clear(); private_key.clear(); password.clear(); }
    };

private:
    TLSVersion                m_min_version;
    TLSVersion                m_max_version;
    std::vector<std::string>  m_cipher_suites;       
    CertificateData           m_certificate;
    std::string               m_ca_store_path;       
    VerifyMode                m_verify_mode;
    bool                      m_initialized;

    mutable std::mutex        m_ctx_mutex;

#ifdef OS_WINDOWS
    CredHandle                m_cred_handle;
    bool                      m_cred_acquired;
    HCERTSTORE                m_cert_store;
    PCCERT_CONTEXT            m_cert_context;
#endif

public:
    TLSContext() noexcept
        : m_min_version(TLSVersion::TLS_1_2),
          m_max_version(TLSVersion::TLS_1_3),
          m_verify_mode(VerifyMode::Peer),
          m_initialized(false)
    #ifdef OS_WINDOWS
        , m_cred_acquired(false),
          m_cert_store(nullptr),
          m_cert_context(nullptr)
    #endif
    {
    #ifdef OS_WINDOWS
        std::memset(&m_cred_handle, 0, sizeof(m_cred_handle));
    #endif
    }

    ~TLSContext() noexcept { release(); }
    TLSContext(const TLSContext&) = delete;
    TLSContext& operator=(const TLSContext&) = delete;

    TLSContext(TLSContext&& other) noexcept
        : m_min_version(other.m_min_version),
          m_max_version(other.m_max_version),
          m_cipher_suites(std::move(other.m_cipher_suites)),
          m_certificate(std::move(other.m_certificate)),
          m_ca_store_path(std::move(other.m_ca_store_path)),
          m_verify_mode(other.m_verify_mode),
          m_initialized(other.m_initialized)
    #ifdef OS_WINDOWS
        , m_cred_handle(other.m_cred_handle),
          m_cred_acquired(other.m_cred_acquired),
          m_cert_store(other.m_cert_store),
          m_cert_context(other.m_cert_context)
    #endif
    {
    #ifdef OS_WINDOWS
        std::memset(&other.m_cred_handle, 0, sizeof(CredHandle));
        other.m_cred_acquired = false;
        other.m_cert_store    = nullptr;
        other.m_cert_context  = nullptr;
    #endif
        other.m_initialized = false;
    }

    TLSContext& operator=(TLSContext&& other) noexcept {
        if (this != &other) {
            release();
            std::lock_guard<std::mutex> lock(m_ctx_mutex);
            m_min_version   = other.m_min_version;
            m_max_version   = other.m_max_version;
            m_cipher_suites = std::move(other.m_cipher_suites);
            m_certificate   = std::move(other.m_certificate);
            m_ca_store_path = std::move(other.m_ca_store_path);
            m_verify_mode   = other.m_verify_mode;
            m_initialized   = other.m_initialized;

        #ifdef OS_WINDOWS
            m_cred_handle   = other.m_cred_handle;
            m_cred_acquired = other.m_cred_acquired;
            m_cert_store    = other.m_cert_store;
            m_cert_context  = other.m_cert_context;
            std::memset(&other.m_cred_handle, 0, sizeof(CredHandle));
            other.m_cred_acquired = false;
            other.m_cert_store    = nullptr;
            other.m_cert_context  = nullptr;
        #endif

            other.m_initialized = false;
        }

        return *this;
    }

    TLSContext& set_protocol_range(TLSVersion min_ver, TLSVersion max_ver) noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_min_version = min_ver;
        m_max_version = max_ver;
        return *this;
    }

    TLSContext& set_cipher_suites(const std::vector<std::string>& suites) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_cipher_suites = suites;
        return *this;
    }

    TLSContext& set_cipher_suites(std::vector<std::string>&& suites) noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_cipher_suites = std::move(suites);
        return *this;
    }

    TLSContext& set_certificate(const CertificateData& cert) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_certificate = cert;
        return *this;
    }

    TLSContext& set_certificate(CertificateData&& cert) noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_certificate = std::move(cert);
        return *this;
    }

    TLSContext& set_ca_store_path(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_ca_store_path = path;
        return *this;
    }

    TLSContext& set_verify_mode(VerifyMode mode) noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_verify_mode = mode;
        return *this;
    }

    bool initialize() noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        if (m_initialized) return true;

    #ifdef OS_WINDOWS
        if (!acquire_schannel_credentials()) return false;
    #endif

        m_initialized = true;
        return true;
    }

    void release() noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        if (!m_initialized) return;

    #ifdef OS_WINDOWS
        release_schannel_resources();
    #endif

        m_initialized = false;
    }

    bool        is_initialized()  const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_initialized; }
    TLSVersion  min_version()     const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_min_version; }
    TLSVersion  max_version()     const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_max_version; }
    VerifyMode  verify_mode()     const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_verify_mode; }

    const std::string& ca_store_path() const noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_ca_store_path;
    }

    const std::vector<std::string>& cipher_suites() const noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_cipher_suites;
    }

    bool has_certificate() const noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return !m_certificate.empty();
    }

#ifdef OS_WINDOWS
    const CredHandle& credential_handle() const noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_cred_handle;
    }

    bool credentials_acquired() const noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_cred_acquired;
    }
#endif

    static const char* version_to_string(TLSVersion v) noexcept {
        switch (v) {
            case TLSVersion::TLS_1_0: return "TLS 1.0";
            case TLSVersion::TLS_1_1: return "TLS 1.1";
            case TLSVersion::TLS_1_2: return "TLS 1.2";
            case TLSVersion::TLS_1_3: return "TLS 1.3";
            default:                  return "Unknown";
        }
    }

    static const char* verify_mode_to_string(VerifyMode m) noexcept {
        switch (m) {
            case VerifyMode::None:           return "None";
            case VerifyMode::Peer:           return "Peer";
            case VerifyMode::FailIfNoPeer:   return "FailIfNoPeer";
            default:                         return "Unknown";
        }
    }

    std::string to_string() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        std::string result = "TLSContext [";
        result += version_to_string(m_min_version);
        result += " - ";
        result += version_to_string(m_max_version);
        result += "] verify=";
        result += verify_mode_to_string(m_verify_mode);
        result += " cert=";
        result += m_certificate.empty() ? "none" : "loaded";
        result += " ca=";
        result += m_ca_store_path.empty() ? "system" : m_ca_store_path;
        result += m_initialized ? " (active)" : " (not initialized)";
        return result;
    }

private:
#ifdef OS_WINDOWS
    DWORD build_protocol_flags() const noexcept {
        DWORD flags = 0;
        auto v_min = static_cast<std::uint32_t>(m_min_version);
        auto v_max = static_cast<std::uint32_t>(m_max_version);
        if (v_min <= 0x0301 && v_max >= 0x0301) flags |= SP_PROT_TLS1_0;
        if (v_min <= 0x0302 && v_max >= 0x0302) flags |= SP_PROT_TLS1_1;
        if (v_min <= 0x0303 && v_max >= 0x0303) flags |= SP_PROT_TLS1_2;

    #ifdef SP_PROT_TLS1_3
        if (v_min <= 0x0304 && v_max >= 0x0304) flags |= SP_PROT_TLS1_3;
    #endif

        return flags;
    }

    bool open_certificate_store() noexcept {
        if (m_certificate.empty()) return true; // No cert needed
        CRYPT_DATA_BLOB pfx_blob;
        pfx_blob.cbData = static_cast<DWORD>(m_certificate.chain.size());
        pfx_blob.pbData = const_cast<BYTE*>(m_certificate.chain.data());
        std::wstring wide_pass(m_certificate.password.begin(), m_certificate.password.end());
        m_cert_store = PFXImportCertStore(&pfx_blob, wide_pass.c_str(), CRYPT_EXPORTABLE | CRYPT_USER_KEYSET);

        if (!m_cert_store) {
            // treat ca_store_path as a system store name
            if (!m_ca_store_path.empty()) {
                std::wstring wide_store(m_ca_store_path.begin(), m_ca_store_path.end());
                m_cert_store = CertOpenSystemStoreW(0, wide_store.c_str());
            } else {
                m_cert_store = CertOpenSystemStoreW(0, L"MY");
            }
        }

        if (!m_cert_store) return false;

        m_cert_context = CertFindCertificateInStore(
            m_cert_store,
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            0,
            CERT_FIND_ANY,
            nullptr,
            nullptr
        );

        return m_cert_context != nullptr || m_certificate.empty();
    }

    bool acquire_schannel_credentials() noexcept {
        if (!open_certificate_store()) return false;
        SCHANNEL_CRED cred;
        std::memset(&cred, 0, sizeof(cred));
        cred.dwVersion = SCHANNEL_CRED_VERSION;
        cred.grbitEnabledProtocols = build_protocol_flags();

        if (m_cert_context) {
            cred.cCreds = 1;
            cred.paCred = &m_cert_context;
        }

        switch (m_verify_mode) {
            case VerifyMode::None:
                cred.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_SERVERNAME_CHECK;
                break;
            case VerifyMode::Peer:
            case VerifyMode::FailIfNoPeer:
                // FailIfNoPeer is enforced at handshake time via ASC_REQ_MUTUAL_AUTH
                // in AcceptSecurityContext; at credential level the flags are identical
                cred.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_REVOCATION_CHECK_CHAIN;
                break;
        }

        TimeStamp expiry;

        SECURITY_STATUS status = AcquireCredentialsHandleW(
            nullptr,
            const_cast<SEC_WCHAR*>(UNISP_NAME_W),
            SECPKG_CRED_OUTBOUND,
            nullptr,
            &cred,
            nullptr,
            nullptr,
            &m_cred_handle,
            &expiry
        );

        m_cred_acquired = (status == SEC_E_OK);
        return m_cred_acquired;
    }

    void release_schannel_resources() noexcept {
        if (m_cred_acquired) {
            FreeCredentialsHandle(&m_cred_handle);
            std::memset(&m_cred_handle, 0, sizeof(m_cred_handle));
            m_cred_acquired = false;
        }

        if (m_cert_context) {
            CertFreeCertificateContext(m_cert_context);
            m_cert_context = nullptr;
        }

        if (m_cert_store) {
            CertCloseStore(m_cert_store, 0);
            m_cert_store = nullptr;
        }
    }
#endif // OS_WINDOWS
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_CONTEXT_HPP