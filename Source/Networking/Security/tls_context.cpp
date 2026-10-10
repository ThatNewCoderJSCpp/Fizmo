#include "fizmo_library.hpp"
#include "tls_context.hpp"

namespace fizmo {
namespace networking {
namespace security {

TLSContext::TLSContext(TLSContext&& other) noexcept : m_initialized(false) {
    std::lock_guard<std::mutex> lock(other.m_ctx_mutex);
    other.release_unlocked();
    m_config = std::move(other.m_config);
}

auto TLSContext::operator=(TLSContext&& other) noexcept -> TLSContext& {
    if (this != &other) {
        std::unique_lock<std::mutex> lk1(m_ctx_mutex, std::defer_lock);
        std::unique_lock<std::mutex> lk2(other.m_ctx_mutex, std::defer_lock);
        std::lock(lk1, lk2);
        release_unlocked();
        other.release_unlocked();
        m_config = std::move(other.m_config);
    }

    return *this;
}

auto TLSContext::set_protocol_range(TLSVersion min_ver, TLSVersion max_ver) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.min_version = min_ver;
    m_config.max_version = max_ver;
    return *this;
}

auto TLSContext::set_cipher_suites(std::vector<std::string>&& suites) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.cipher_suites = std::move(suites);
    return *this;
}

auto TLSContext::set_certificate(CertificateData&& cert) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.certificate = std::move(cert);
    return *this;
}

auto TLSContext::set_check_revocation(bool enable) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.check_revocation = enable;
    return *this;
}

bool TLSContext::initialize() noexcept {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    if (m_initialized) return true;
    if (!m_credentials.acquire(m_config)) return false;
    m_initialized = true;
    return true;
}

std::string TLSContext::to_string() const {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    std::string result = "TLSContext [";
    result += tls_version_to_string(m_config.min_version);
    result += " - ";
    result += tls_version_to_string(m_config.max_version);
    result += "] backend=";
    result += detail::kBackendName;
    result += " use=";
    result += (m_config.use == ContextUse::Server) ? "server" : "client";
    result += " verify=";
    result += security::verify_mode_to_string(m_config.verify_mode);
    result += " cert=";
    result += m_config.certificate.empty() ? "none" : "loaded";
    result += " ca=";
    result += m_config.ca_store_path.empty() ? "system" : m_config.ca_store_path;
    result += m_initialized ? " (active)" : " (not initialized)";
    return result;
}

} // namespace security
} // namespace networking
} // namespace fizmo
