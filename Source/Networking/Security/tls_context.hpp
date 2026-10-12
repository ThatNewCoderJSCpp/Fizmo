#ifndef FIZMO_TLS_CONTEXT_HPP
#define FIZMO_TLS_CONTEXT_HPP

#include "Backend/native_backend.hpp"
#include "cipher_suites.hpp"
#include "tls_policy.hpp"
#include "tls_session_cache.hpp"
#include "tls_metrics.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>

namespace fizmo {
namespace networking {
namespace security {

class Certificate;
class CertificateChain;
class CertificateVerifier;
class PrivateKey;
class PinSet;
class TLSContext;

using SNICallback             = std::function<TLSContext*(const std::string& server_name)>;
using PeerCertificateCallback = std::function<bool(const CertificateChain& chain)>;

class TLSContext {
public:
    using CertificateData = security::CertificateData;

    TLSContext() noexcept;
    explicit TLSContext(ContextUse use) noexcept;
    ~TLSContext() noexcept { release(); }

    TLSContext(const TLSContext&) = delete;
    TLSContext& operator=(const TLSContext&) = delete;
    TLSContext(TLSContext&& other) noexcept;
    TLSContext& operator=(TLSContext&& other) noexcept;

public:
    TLSContext& set_protocol_range(TLSVersion min_ver, TLSVersion max_ver) noexcept;
    TLSContext& set_cipher_suites(const std::vector<std::string>& suites) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_config.cipher_suites = suites;
        return *this;
    }
    TLSContext& set_cipher_suites(std::vector<std::string>&& suites) noexcept;
    TLSContext& set_cipher_list(const CipherList& list);
    TLSContext& set_schannel_algorithms(const std::vector<unsigned>& algs);
    TLSContext& set_certificate(const CertificateData& cert) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_config.certificate = cert;
        return *this;
    }
    TLSContext& set_certificate(CertificateData&& cert) noexcept;
    bool set_certificate(const Certificate& leaf, const PrivateKey& key, const std::vector<const Certificate*>& chain = {});
    bool set_certificate_chain(const CertificateChain& chain, const PrivateKey& key);
    bool set_certificate_files(const std::string& cert_pem_path, const std::string& key_path, const std::string& passphrase = {});
    bool set_pkcs12_file(const std::string& path, const std::string& password);
    TLSContext& set_ca_store_path(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_config.ca_store_path = path;
        return *this;
    }
    TLSContext& set_verify_mode(VerifyMode mode) noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_config.verify_mode = mode;
        return *this;
    }
    TLSContext& set_use(ContextUse use) noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_config.use = use;
        return *this;
    }
    TLSContext& set_check_revocation(bool enable) noexcept;

    TLSContext& set_alpn_protocols(const std::vector<std::string>& protocols);
    TLSContext& add_crl_file(const std::string& path);
    TLSContext& set_crl_files(const std::vector<std::string>& paths);
    TLSContext& set_session_tickets(bool enable) noexcept;
    TLSContext& set_session_timeout(std::uint32_t seconds) noexcept;
    TLSContext& set_session_resumption(bool enable) noexcept;
    TLSContext& set_request_ocsp(bool enable) noexcept;
    TLSContext& set_require_ocsp(bool require) noexcept;
    TLSContext& set_ocsp_staple(const std::vector<std::uint8_t>& response);
    TLSContext& set_post_handshake_auth(bool enable) noexcept;
    TLSContext& set_allow_renegotiation(bool enable) noexcept;
    TLSContext& set_allow_insecure_versions(bool enable) noexcept;
    TLSContext& set_keylog_file(const std::string& path);
    TLSContext& set_keylog_callback(std::function<void(const std::string&)> cb);
    bool use_preset(ProtocolPreset preset, bool allow_insecure = false);

    TLSContext& set_session_cache(std::shared_ptr<TLSSessionCache> cache);
    TLSContext& set_metrics(std::shared_ptr<TLSMetricsCollector> metrics);
    TLSContext& set_policy(const TLSPolicy& policy);
    TLSContext& set_sni_callback(SNICallback cb);
    TLSContext& set_peer_certificate_callback(PeerCertificateCallback cb);
    TLSContext& set_verifier(std::shared_ptr<CertificateVerifier> verifier);
    TLSContext& set_pins(std::shared_ptr<PinSet> pins);

public:
    bool initialize() noexcept;
    void release() noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        release_unlocked();
    }

public:
    bool       is_initialized() const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_initialized; }
    TLSVersion min_version()    const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_config.min_version; }
    TLSVersion max_version()    const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_config.max_version; }
    VerifyMode verify_mode()    const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_config.verify_mode; }
    ContextUse use()            const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_config.use; }
    bool       check_revocation() const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_config.check_revocation; }
    bool       session_resumption() const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_session_resumption; }
    bool       require_ocsp() const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_require_ocsp; }
    bool       request_ocsp() const noexcept { std::lock_guard<std::mutex> lock(m_ctx_mutex); return m_config.request_ocsp; }
    std::string ca_store_path() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_config.ca_store_path;
    }
    std::vector<std::string> cipher_suites() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_config.cipher_suites;
    }
    std::vector<std::string> alpn_protocols() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_config.alpn_protocols;
    }
    std::vector<std::string> crl_files() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_config.crl_paths;
    }
    bool has_certificate() const noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return !m_config.certificate.empty();
    }
    std::string last_error() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_error.empty() ? m_credentials.last_error() : m_error;
    }
    TLSConfig config() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_config;
    }

    std::shared_ptr<TLSSessionCache>     session_cache() const;
    std::shared_ptr<TLSMetricsCollector> metrics() const;
    TLSPolicy                            policy() const;
    SNICallback                          sni_callback() const;
    PeerCertificateCallback              peer_certificate_callback() const;
    std::shared_ptr<CertificateVerifier> verifier() const;
    std::shared_ptr<PinSet>              pins() const;

    static const char* backend_name() noexcept { return detail::kBackendName; }
    static bool keylog_supported() noexcept { return detail::NativeCredentials::keylog_supported(); }

public:
    detail::NativeCredentials& credentials() noexcept { return m_credentials; }
    static const char* version_to_string(TLSVersion v) noexcept { return tls_version_to_string(v); }
    static const char* verify_mode_to_string(VerifyMode m) noexcept { return security::verify_mode_to_string(m); }
    std::string to_string() const;

private:
    TLSConfig                            m_config;
    detail::NativeCredentials            m_credentials;
    bool                                 m_initialized;
    bool                                 m_session_resumption = true;
    bool                                 m_require_ocsp = false;
    std::string                          m_error;
    mutable std::shared_ptr<TLSSessionCache>     m_session_cache;
    mutable std::shared_ptr<TLSMetricsCollector> m_metrics;
    TLSPolicy                            m_policy;
    SNICallback                          m_sni;
    PeerCertificateCallback              m_peer_cert_cb;
    std::shared_ptr<CertificateVerifier> m_verifier;
    std::shared_ptr<PinSet>              m_pins;
    mutable std::mutex                   m_ctx_mutex;

    void release_unlocked() noexcept {
        if (!m_initialized) return;
        m_credentials.release();
        m_initialized = false;
    }
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_CONTEXT_HPP
