#ifndef FIZMO_SCHANNEL_BACKEND_HPP
#define FIZMO_SCHANNEL_BACKEND_HPP

#include "backend_base.hpp"

#ifdef OS_WINDOWS

#include <wincrypt.h>
#include <schannel.h>
#define SECURITY_WIN32
#include <security.h>
#include <sspi.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")

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
        bool check_revocation
    ) noexcept;

private:
    static HCERTSTORE build_extra_store(
        const std::vector<const NativeCertificateBase*>& extras,
        const std::string& ca_store_path
    ) noexcept;

    static VerifyResult inspect_chain_status(PCCERT_CHAIN_CONTEXT chain_ctx) noexcept;

    static VerifyResult map_policy_error(DWORD err, int depth) noexcept;

    static int error_depth(PCCERT_CHAIN_CONTEXT chain_ctx, DWORD flag) noexcept;

    static int chain_length(PCCERT_CHAIN_CONTEXT chain_ctx) noexcept;
};

class SchannelCredentials : public NativeCredentialsBase {
private:
    CredHandle     m_cred{};
    bool           m_acquired    = false;
    HCERTSTORE     m_cert_store  = nullptr;
    PCCERT_CONTEXT m_cert_ctx    = nullptr;
    std::string    m_last_error;

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

    std::vector<std::uint8_t>  m_pending;

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

private:
    static DWORD client_flags() noexcept;

    static DWORD server_flags() noexcept;

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