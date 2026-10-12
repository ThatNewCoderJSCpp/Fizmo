#ifndef FIZMO_SCHANNEL_BACKEND_HPP
#define FIZMO_SCHANNEL_BACKEND_HPP

#include "backend_base.hpp"

#ifdef OS_WINDOWS

#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <schannel.h>
#define SECURITY_WIN32
#include <security.h>
#include <sspi.h>
#include <ncrypt.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <mutex>

#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "bcrypt.lib")

namespace fizmo {
namespace networking {
namespace security {
namespace detail {

std::wstring utf8_to_wide(const std::string& utf8) noexcept;

std::string wide_to_utf8(const wchar_t* wide) noexcept;

std::string hex32(unsigned long value);

class SchannelCertificate : public NativeCertificateBase {
private:
    PCCERT_CONTEXT  m_ctx = nullptr;
    CertificateInfo m_info;

public:
    SchannelCertificate() noexcept = default;
    ~SchannelCertificate() noexcept override;

    SchannelCertificate(const SchannelCertificate&) = delete;
    SchannelCertificate& operator=(const SchannelCertificate&) = delete;

    SchannelCertificate(SchannelCertificate&& other) noexcept;

    SchannelCertificate& operator=(SchannelCertificate&& other) noexcept;

public:
    bool parse(const std::uint8_t* der, std::size_t len) noexcept override;

    void release() noexcept override;

    bool valid() const noexcept override;
    const CertificateInfo& info() const noexcept override;
    void* native() const noexcept override;

    bool self_signed() const noexcept override;

    bool adopt(PCCERT_CONTEXT ctx) noexcept;

private:
    void fill_info() noexcept;

    std::string name_string(DWORD type, DWORD flags) const noexcept;

    static std::string format_serial(const CRYPT_INTEGER_BLOB& blob);

    static std::int64_t filetime_to_unix(const FILETIME& ft) noexcept;

    void parse_sans() noexcept;

    void parse_key_usage() noexcept;
};

class SchannelCertOps {
public:
    static bool issuer_links_to_subject(
        const NativeCertificateBase& child,
        const NativeCertificateBase& issuer
    ) noexcept;

    static VerifyResult verify_chain(
        const NativeCertificateBase& leaf,
        const std::vector<const NativeCertificateBase*>& extras,
        const std::string& hostname,
        const std::string& ca_store_path,
        bool check_revocation,
        const std::vector<std::string>& crl_paths = {}
    ) noexcept;

    static OCSPResult verify_ocsp(
        const std::vector<std::uint8_t>& response,
        const NativeCertificateBase& leaf,
        const NativeCertificateBase* issuer,
        const std::string& ca_store_path
    ) noexcept;

    static bool load_system_roots(std::vector<std::vector<std::uint8_t>>& out, const std::string& store_name) noexcept;

    static int load_file_into_store(const std::string& path, HCERTSTORE certs, HCERTSTORE crls) noexcept;

private:
    static HCERTSTORE build_extra_store(
        const std::vector<const NativeCertificateBase*>& extras,
        const std::string& ca_store_path,
        HCERTSTORE* roots
    ) noexcept;

    static VerifyResult check_crls(PCCERT_CHAIN_CONTEXT chain_ctx, const std::vector<std::string>& crl_paths) noexcept;

    static VerifyResult inspect_chain_status(PCCERT_CHAIN_CONTEXT chain_ctx, DWORD ignored = 0) noexcept;
    static bool root_in_store(PCCERT_CHAIN_CONTEXT chain_ctx, HCERTSTORE roots) noexcept;

    static VerifyResult map_policy_error(DWORD err, int depth) noexcept;

    static int error_depth(PCCERT_CHAIN_CONTEXT chain_ctx, DWORD flag) noexcept;

    static int chain_length(PCCERT_CHAIN_CONTEXT chain_ctx) noexcept;
};

class SchannelPrivateKey : public NativePrivateKeyBase {
private:
    NCRYPT_PROV_HANDLE m_prov = 0;
    NCRYPT_KEY_HANDLE  m_key  = 0;
    std::string        m_last_error;

    bool import_pkcs8(const std::uint8_t* der, std::size_t len, const std::string& passphrase) noexcept;
    bool export_public(std::vector<std::uint8_t>& out) const noexcept;

public:
    SchannelPrivateKey() noexcept = default;
    ~SchannelPrivateKey() noexcept override { release(); }

    SchannelPrivateKey(const SchannelPrivateKey&) = delete;
    SchannelPrivateKey& operator=(const SchannelPrivateKey&) = delete;
    SchannelPrivateKey(SchannelPrivateKey&& o) noexcept : m_prov(o.m_prov), m_key(o.m_key), m_last_error(std::move(o.m_last_error)) { o.m_prov = 0; o.m_key = 0; }
    SchannelPrivateKey& operator=(SchannelPrivateKey&& o) noexcept {
        if (this != &o) { release(); m_prov = o.m_prov; m_key = o.m_key; o.m_prov = 0; o.m_key = 0; m_last_error = std::move(o.m_last_error); }
        return *this;
    }

    bool load_pem(const std::string& pem, const std::string& passphrase) noexcept override;
    bool load_der(const std::uint8_t* der, std::size_t len, const std::string& passphrase) noexcept override;
    void release() noexcept override;
    bool valid() const noexcept override { return m_key != 0; }
    KeyType key_type() const noexcept override;
    int key_bits() const noexcept override;
    bool matches_certificate(const NativeCertificateBase& cert) const noexcept override;
    bool to_pkcs8_der(std::vector<std::uint8_t>& out) const noexcept override;
    bool build_pkcs12(const NativeCertificateBase& leaf, const std::vector<const NativeCertificateBase*>& chain,
                      const std::string& password, std::vector<std::uint8_t>& out) const noexcept override;
    void* native() const noexcept override { return reinterpret_cast<void*>(m_key); }
    const std::string& last_error() const noexcept override { return m_last_error; }
};

class SchannelCredentials : public NativeCredentialsBase {
private:
    CredHandle     m_cred{};
    bool           m_acquired    = false;
    HCERTSTORE     m_cert_store  = nullptr;
    PCCERT_CONTEXT m_cert_ctx    = nullptr;
    std::string    m_last_error;
    TLSConfig      m_config;

    bool acquire_modern(DWORD flags, unsigned long use) noexcept;
    bool acquire_legacy(DWORD flags, unsigned long use) noexcept;

public:
    SchannelCredentials() noexcept;
    ~SchannelCredentials() noexcept override;

    SchannelCredentials(const SchannelCredentials&) = delete;
    SchannelCredentials& operator=(const SchannelCredentials&) = delete;

public:
    bool acquire(const TLSConfig& config) noexcept override;

    void release() noexcept override;

    bool acquired() const noexcept override;
    const std::string& last_error() const noexcept override;
    void* native() const noexcept override;
    const TLSConfig& config() const noexcept { return m_config; }

    static bool keylog_supported() noexcept { return false; }

private:
    static DWORD protocol_flags(const TLSConfig& config) noexcept;

    bool open_certificate_store(const TLSConfig& config) noexcept;
};

class SchannelSession : public NativeSessionBase {
private:
    CtxtHandle                 m_ctx{};
    CredHandle*                m_cred = nullptr;
    bool                       m_established = false;
    bool                       m_handshake_done = false;
    bool                       m_server = false;
    std::string                m_hostname;
    SecPkgContext_StreamSizes  m_sizes{};
    bool                       m_sizes_queried = false;
    const SchannelCredentials* m_creds = nullptr;
    std::vector<std::uint8_t>  m_alpn_buffer;
    AlertInfo                  m_alert;

    std::vector<std::uint8_t>  m_pending;

    void note_failure(SECURITY_STATUS status) noexcept;

public:
    SchannelSession() noexcept;

    ~SchannelSession() noexcept override;

    SchannelSession(const SchannelSession&) = delete;
    SchannelSession& operator=(const SchannelSession&) = delete;

public:
    bool open(NativeCredentialsBase& credentials, bool server, const std::string& hostname) noexcept override;

    void close_session() noexcept override;

    bool established() const noexcept override;

public:
    HandshakeIO handshake(const std::uint8_t* input, std::size_t length) noexcept override;

    int encrypt(const std::uint8_t* plaintext, std::size_t length, std::vector<std::uint8_t>& out) noexcept override;

    DecryptIO decrypt(const std::uint8_t* ciphertext, std::size_t length) noexcept override;

    bool close_notify(std::vector<std::uint8_t>& out) noexcept override;

    bool peer_chain(std::vector<std::vector<std::uint8_t>>& out) const noexcept override;

    std::string cipher_name() const noexcept override;

    TLSVersion protocol_version() const noexcept override;

    std::size_t max_plaintext_chunk() const noexcept override;

    std::string alpn_selected() const noexcept override;
    bool        session_reused() const noexcept override;
    bool export_keying_material(const std::string& label, const std::uint8_t* context, std::size_t context_len, bool use_context, std::size_t out_len, std::vector<std::uint8_t>& out) const noexcept override;
    bool channel_binding(ChannelBindingType type, std::vector<std::uint8_t>& out) const noexcept override;
    AlertInfo last_alert() const noexcept override { return m_alert; }
    bool local_certificate(std::vector<std::uint8_t>& out) const noexcept override;

    static std::uint8_t status_to_alert(SECURITY_STATUS status) noexcept;

private:
    static DWORD client_flags() noexcept;

    DWORD server_flags() const noexcept;

    bool ensure_sizes() noexcept;

    void take_extra(std::size_t extra, std::vector<std::uint8_t>& out) noexcept;

    static void collect_extra(
        SecBuffer bufs[4],
        const std::vector<std::uint8_t>& work,
        std::vector<std::uint8_t>& out
    ) noexcept;
};

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo

#endif // OS_WINDOWS
#endif // FIZMO_SCHANNEL_BACKEND_HPP