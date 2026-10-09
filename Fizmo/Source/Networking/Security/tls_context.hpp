#ifndef FIZMO_TLS_CONTEXT_HPP
#define FIZMO_TLS_CONTEXT_HPP

#include "Backend/native_backend.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <memory>

namespace fizmo {
namespace networking {
namespace security {

class TLSContext {
public:
    using CertificateData = security::CertificateData;

    TLSContext() noexcept : m_initialized(false) {}
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

    TLSContext& set_certificate(const CertificateData& cert) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_config.certificate = cert;
        return *this;
    }

    TLSContext& set_certificate(CertificateData&& cert) noexcept;

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

    std::string ca_store_path() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_config.ca_store_path;
    }

    std::vector<std::string> cipher_suites() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_config.cipher_suites;
    }

    bool has_certificate() const noexcept {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return !m_config.certificate.empty();
    }

    std::string last_error() const {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        return m_credentials.last_error();
    }

    static const char* backend_name() noexcept { return detail::kBackendName; }

public:
    detail::NativeCredentials& credentials() noexcept { return m_credentials; }

    static const char* version_to_string(TLSVersion v) noexcept { return tls_version_to_string(v); }
    static const char* verify_mode_to_string(VerifyMode m) noexcept { return security::verify_mode_to_string(m); }

    std::string to_string() const;

private:
    TLSConfig                 m_config;
    detail::NativeCredentials m_credentials;
    bool                      m_initialized;
    mutable std::mutex        m_ctx_mutex;

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