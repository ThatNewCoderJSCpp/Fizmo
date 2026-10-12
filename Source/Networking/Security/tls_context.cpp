#include "fizmo_library.hpp"
#include "tls_context.hpp"
#include "certificate.hpp"
#include "certificate_chain.hpp"
#include "certificate_verifier.hpp"
#include "certificate_pinning.hpp"
#include "private_key.hpp"
#include "../../Crypto/encoding.hpp"

#include <fstream>
#include <iterator>

namespace fizmo {
namespace networking {
namespace security {

TLSContext::TLSContext() noexcept : m_initialized(false) {}

TLSContext::TLSContext(ContextUse use) noexcept : m_initialized(false) {
    m_config.use = use;
    if (use == ContextUse::Server) m_config.verify_mode = VerifyMode::None;
}

TLSContext::TLSContext(TLSContext&& other) noexcept : m_initialized(false) {
    std::lock_guard<std::mutex> lock(other.m_ctx_mutex);
    other.release_unlocked();
    m_config             = std::move(other.m_config);
    m_session_resumption = other.m_session_resumption;
    m_require_ocsp       = other.m_require_ocsp;
    m_session_cache      = std::move(other.m_session_cache);
    m_metrics            = std::move(other.m_metrics);
    m_policy             = other.m_policy;
    m_sni                = std::move(other.m_sni);
    m_peer_cert_cb       = std::move(other.m_peer_cert_cb);
    m_verifier           = std::move(other.m_verifier);
    m_pins               = std::move(other.m_pins);
}

auto TLSContext::operator=(TLSContext&& other) noexcept -> TLSContext& {
    if (this != &other) {
        std::unique_lock<std::mutex> lk1(m_ctx_mutex, std::defer_lock);
        std::unique_lock<std::mutex> lk2(other.m_ctx_mutex, std::defer_lock);
        std::lock(lk1, lk2);
        release_unlocked();
        other.release_unlocked();
        m_config             = std::move(other.m_config);
        m_session_resumption = other.m_session_resumption;
        m_require_ocsp       = other.m_require_ocsp;
        m_session_cache      = std::move(other.m_session_cache);
        m_metrics            = std::move(other.m_metrics);
        m_policy             = other.m_policy;
        m_sni                = std::move(other.m_sni);
        m_peer_cert_cb       = std::move(other.m_peer_cert_cb);
        m_verifier           = std::move(other.m_verifier);
        m_pins               = std::move(other.m_pins);
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

auto TLSContext::set_cipher_list(const CipherList& list) -> TLSContext& {
    auto names = list.native_names();
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.cipher_suites = std::move(names);
    return *this;
}

auto TLSContext::set_schannel_algorithms(const std::vector<unsigned>& algs) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.native_algorithms = algs;
    return *this;
}

auto TLSContext::set_certificate(CertificateData&& cert) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.certificate = std::move(cert);
    return *this;
}

bool TLSContext::set_certificate(const Certificate& leaf, const PrivateKey& key, const std::vector<const Certificate*>& chain) {
    if (!leaf.loaded() || !key.loaded()) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_error = "Certificate or private key not loaded";
        return false;
    }

    if (!key.matches_certificate(leaf)) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_error = "Private key does not match the certificate";
        return false;
    }

    const std::string password = crypto::random_token(16);
    std::vector<std::uint8_t> p12;

    if (!key.to_pkcs12(leaf, chain, password, p12)) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_error = "Could not build a PKCS#12 bundle: " + key.last_error();
        return false;
    }

    CertificateData data;
    data.chain = std::move(p12);
    data.password = password;
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.certificate = std::move(data);
    m_error.clear();
    return true;
}

bool TLSContext::set_certificate_chain(const CertificateChain& chain, const PrivateKey& key) {
    if (chain.empty()) return false;
    return set_certificate(chain.leaf(), key, chain.tail());
}

bool TLSContext::set_certificate_files(const std::string& cert_pem_path, const std::string& key_path, const std::string& passphrase) {
    CertificateChain chain = CertificateChain::from_pem_bundle(cert_pem_path);

    if (chain.empty()) {
        Certificate single;

        if (single.load_from_file(cert_pem_path)) {
            chain.push_back(std::move(single));
        } else {
            std::lock_guard<std::mutex> lock(m_ctx_mutex);
            m_error = "Could not load certificate file \"" + cert_pem_path + "\"";
            return false;
        }
    }

    PrivateKey key;

    if (!key.load_from_file(key_path, passphrase)) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_error = "Could not load private key \"" + key_path + "\": " + key.last_error();
        return false;
    }

    return set_certificate_chain(chain, key);
}

bool TLSContext::set_pkcs12_file(const std::string& path, const std::string& password) {
    std::ifstream f(path, std::ios::binary);

    if (!f) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_error = "Could not open \"" + path + "\"";
        return false;
    }

    CertificateData data;
    data.chain.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    data.password = password;
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.certificate = std::move(data);
    return true;
}

auto TLSContext::set_check_revocation(bool enable) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.check_revocation = enable;
    return *this;
}

auto TLSContext::set_alpn_protocols(const std::vector<std::string>& protocols) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.alpn_protocols = protocols;
    return *this;
}

auto TLSContext::add_crl_file(const std::string& path) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.crl_paths.push_back(path);
    return *this;
}

auto TLSContext::set_crl_files(const std::vector<std::string>& paths) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.crl_paths = paths;
    return *this;
}

auto TLSContext::set_session_tickets(bool enable) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.session_tickets = enable;
    return *this;
}

auto TLSContext::set_session_timeout(std::uint32_t seconds) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.session_timeout_seconds = seconds;
    return *this;
}

auto TLSContext::set_session_resumption(bool enable) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_session_resumption = enable;
    return *this;
}

auto TLSContext::set_request_ocsp(bool enable) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.request_ocsp = enable;
    return *this;
}

auto TLSContext::set_require_ocsp(bool require) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_require_ocsp = require;
    if (require) m_config.request_ocsp = true;
    return *this;
}

auto TLSContext::set_ocsp_staple(const std::vector<std::uint8_t>& response) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.ocsp_staple = response;
    return *this;
}

auto TLSContext::set_post_handshake_auth(bool enable) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.post_handshake_auth = enable;
    return *this;
}

auto TLSContext::set_allow_renegotiation(bool enable) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.allow_renegotiation = enable;
    return *this;
}

auto TLSContext::set_allow_insecure_versions(bool enable) noexcept -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.allow_insecure_versions = enable;
    return *this;
}

auto TLSContext::set_keylog_file(const std::string& path) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.keylog_path = path;
    return *this;
}

auto TLSContext::set_keylog_callback(std::function<void(const std::string&)> cb) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.keylog_callback = std::move(cb);
    return *this;
}

bool TLSContext::use_preset(ProtocolPreset preset, bool allow_insecure) {
    PresetSettings s = preset_settings(preset);

    if (s.insecure && !allow_insecure) {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        m_error = "The Legacy preset enables TLS 1.0/1.1 and must be requested with allow_insecure = true";
        return false;
    }

    auto names = s.ciphers.native_names();
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_config.min_version = s.min_version;
    m_config.max_version = s.max_version;
    m_config.cipher_suites = std::move(names);
    m_config.allow_insecure_versions = s.insecure;
    m_error.clear();
    return true;
}

auto TLSContext::set_session_cache(std::shared_ptr<TLSSessionCache> cache) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_session_cache = std::move(cache);
    return *this;
}

auto TLSContext::set_metrics(std::shared_ptr<TLSMetricsCollector> metrics) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_metrics = std::move(metrics);
    return *this;
}

auto TLSContext::set_policy(const TLSPolicy& policy) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_policy = policy;

    if (m_session_cache) {
        m_session_cache->set_capacity(policy.max_cached_sessions);
        m_session_cache->set_ttl(policy.session_lifetime);
    }

    return *this;
}

auto TLSContext::set_sni_callback(SNICallback cb) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_sni = std::move(cb);
    return *this;
}

auto TLSContext::set_peer_certificate_callback(PeerCertificateCallback cb) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_peer_cert_cb = std::move(cb);
    return *this;
}

auto TLSContext::set_verifier(std::shared_ptr<CertificateVerifier> verifier) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_verifier = std::move(verifier);
    return *this;
}

auto TLSContext::set_pins(std::shared_ptr<PinSet> pins) -> TLSContext& {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    m_pins = std::move(pins);
    return *this;
}

std::shared_ptr<TLSSessionCache> TLSContext::session_cache() const {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    if (!m_session_resumption) return nullptr;
    if (!m_session_cache) m_session_cache = std::make_shared<TLSSessionCache>(m_policy.max_cached_sessions, m_policy.session_lifetime);
    return m_session_cache;
}

std::shared_ptr<TLSMetricsCollector> TLSContext::metrics() const {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    if (!m_metrics) m_metrics = std::make_shared<TLSMetricsCollector>();
    return m_metrics;
}

TLSPolicy TLSContext::policy() const {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    return m_policy;
}

SNICallback TLSContext::sni_callback() const {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    return m_sni;
}

PeerCertificateCallback TLSContext::peer_certificate_callback() const {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    return m_peer_cert_cb;
}

std::shared_ptr<CertificateVerifier> TLSContext::verifier() const {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    return m_verifier;
}

std::shared_ptr<PinSet> TLSContext::pins() const {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    return m_pins;
}

bool TLSContext::initialize() noexcept {
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    if (m_initialized) return true;
    m_error.clear();
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

    if (!m_config.alpn_protocols.empty()) {
        result += " alpn=";

        for (std::size_t i = 0; i < m_config.alpn_protocols.size(); ++i) {
            if (i) result += ',';
            result += m_config.alpn_protocols[i];
        }
    }

    result += m_initialized ? " (active)" : " (not initialized)";
    return result;
}

} // namespace security
} // namespace networking
} // namespace fizmo
