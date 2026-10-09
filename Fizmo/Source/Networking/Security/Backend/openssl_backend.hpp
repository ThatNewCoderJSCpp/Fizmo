#ifndef FIZMO_OPENSSL_BACKEND_HPP
#define FIZMO_OPENSSL_BACKEND_HPP

#include "backend_base.hpp"

#ifdef OS_LINUX

#include <arpa/inet.h>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <algorithm>

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    #define FIZMO_SSL_GET1_PEER_CERT SSL_get_peer_certificate
#else
    #define FIZMO_SSL_GET1_PEER_CERT SSL_get1_peer_certificate
#endif

namespace fizmo {
namespace networking {
namespace security {
namespace detail {

 void openssl_init_once() noexcept;

 std::string openssl_last_error() noexcept;

inline void openssl_drain_errors() noexcept { ERR_clear_error(); }

class OpenSSLCertificate : public NativeCertificateBase {
private:
    X509*           m_x509 = nullptr;
    CertificateInfo m_info;

public:
    OpenSSLCertificate() noexcept { openssl_init_once(); }
    ~OpenSSLCertificate() noexcept override { release(); }

    OpenSSLCertificate(const OpenSSLCertificate&) = delete;
    OpenSSLCertificate& operator=(const OpenSSLCertificate&) = delete;

    OpenSSLCertificate(OpenSSLCertificate&& other) noexcept : m_x509(other.m_x509), m_info(std::move(other.m_info)) {
        other.m_x509 = nullptr;
    }

    OpenSSLCertificate& operator=(OpenSSLCertificate&& other) noexcept;

public:
    bool parse(const std::uint8_t* der, std::size_t len) noexcept override;

    void release() noexcept override {
        if (m_x509) { X509_free(m_x509); m_x509 = nullptr; }
        m_info.clear();
    }

    bool valid() const noexcept override { return m_x509 != nullptr; }
    const CertificateInfo& info() const noexcept override { return m_info; }
    void* native() const noexcept override { return m_x509; }

    bool self_signed() const noexcept override;

    bool adopt(X509* cert) noexcept {
        release();
        if (!cert) return false;
        X509_up_ref(cert);
        m_x509 = cert;
        fill_info();
        return true;
    }

private:
    void fill_info() noexcept;

    static std::string name_to_string(X509_NAME* name) noexcept;

    std::string serial_to_string() const noexcept;

    static std::int64_t asn1_time_to_unix(const ASN1_TIME* t) noexcept;

    void parse_sans() noexcept;

    void parse_key_usage() noexcept;
};

class OpenSSLCertOps {
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
    static VerifyResult map_verify_error(int error, int depth) noexcept;
};

class OpenSSLCredentials : public NativeCredentialsBase {
private:
    SSL_CTX*    m_ctx = nullptr;
    std::string m_last_error;

public:
    OpenSSLCredentials() noexcept { openssl_init_once(); }
    ~OpenSSLCredentials() noexcept override { release(); }

    OpenSSLCredentials(const OpenSSLCredentials&) = delete;
    OpenSSLCredentials& operator=(const OpenSSLCredentials&) = delete;

public:
    bool acquire(const TLSConfig& config) noexcept override;

    void release() noexcept override {
        if (m_ctx) { SSL_CTX_free(m_ctx); m_ctx = nullptr; }
    }

    bool acquired() const noexcept override { return m_ctx != nullptr; }
    const std::string& last_error() const noexcept override { return m_last_error; }
    void* native() const noexcept override { return m_ctx; }

private:
    bool apply_cipher_suites(const TLSConfig& config) noexcept;

    bool apply_trust_store(const TLSConfig& config) noexcept;

    bool apply_certificate(const TLSConfig& config) noexcept;

    bool load_pkcs12(const TLSConfig& config) noexcept;

    bool load_der_pair(const TLSConfig& config) noexcept;
};

class OpenSSLSession : public NativeSessionBase {
private:
    SSL*        m_ssl   = nullptr;
    BIO*        m_rbio  = nullptr;  
    BIO*        m_wbio  = nullptr;   
    bool        m_server = false;
    bool        m_done   = false;
    std::string m_hostname;

public:
    OpenSSLSession() noexcept { openssl_init_once(); }
    ~OpenSSLSession() noexcept override { close_session(); }

    OpenSSLSession(const OpenSSLSession&) = delete;
    OpenSSLSession& operator=(const OpenSSLSession&) = delete;

public:
    bool open(NativeCredentialsBase& credentials, bool server, const std::string& hostname) noexcept override;

    void close_session() noexcept override {
        if (m_ssl) { SSL_free(m_ssl); m_ssl = nullptr; }  
        m_rbio = m_wbio = nullptr;
        m_done = false;
    }

    bool established() const noexcept override { return m_done; }

public:
    HandshakeIO handshake(const std::uint8_t* input, std::size_t length) noexcept override;

    int encrypt(const std::uint8_t* plaintext, std::size_t length, std::vector<std::uint8_t>& out) noexcept override;

    DecryptIO decrypt(const std::uint8_t* ciphertext, std::size_t length) noexcept override;

    bool close_notify(std::vector<std::uint8_t>& out) noexcept override;

    bool peer_chain(std::vector<std::vector<std::uint8_t>>& out) const noexcept override;

    std::string cipher_name() const noexcept override;

    TLSVersion protocol_version() const noexcept override;

    std::size_t max_plaintext_chunk() const noexcept override { return 16384; }

private:
    void drain_wbio(std::vector<std::uint8_t>& out) noexcept;

    static bool append_der(X509* cert, std::vector<std::vector<std::uint8_t>>& out) noexcept;

    static std::string describe_failure(int reason) noexcept;
};

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo

#endif // OS_LINUX
#endif // FIZMO_OPENSSL_BACKEND_HPP