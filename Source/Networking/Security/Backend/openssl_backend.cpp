#include "fizmo_library.hpp"
#include "openssl_backend.hpp"

namespace fizmo {
namespace networking {
namespace security {
namespace detail {

#if defined(OS_LINUX)
void openssl_init_once() noexcept {
    static const bool done = [] {
        OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
        return true;
    }();

    (void)done;
}
#endif

#if defined(OS_LINUX)
std::string openssl_last_error() noexcept {
    unsigned long code = ERR_get_error();
    if (code == 0) return {};
    char buf[256];
    ERR_error_string_n(code, buf, sizeof(buf));
    ERR_clear_error();
    return buf;
}
#endif

#if defined(OS_LINUX)
auto OpenSSLCertificate::operator=(OpenSSLCertificate&& other) noexcept -> OpenSSLCertificate& {
    if (this != &other) {
        release();
        m_x509 = other.m_x509;
        m_info = std::move(other.m_info);
        other.m_x509 = nullptr;
    }

    return *this;
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCertificate::parse(const std::uint8_t* der, std::size_t len) noexcept {
    release();
    if (!der || len == 0) return false;
    const unsigned char* p = der;
    m_x509 = d2i_X509(nullptr, &p, static_cast<long>(len));
    if (!m_x509) { openssl_drain_errors(); return false; }
    fill_info();
    return true;
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCertificate::self_signed() const noexcept {
    if (!m_x509) return false;
    return X509_NAME_cmp(X509_get_subject_name(m_x509), X509_get_issuer_name(m_x509)) == 0;
}
#endif

#if defined(OS_LINUX)
void OpenSSLCertificate::fill_info() noexcept {
    m_info.clear();
    m_info.subject = name_to_string(X509_get_subject_name(m_x509));
    m_info.issuer  = name_to_string(X509_get_issuer_name(m_x509));
    m_info.serial  = serial_to_string();
    m_info.not_before = asn1_time_to_unix(X509_get0_notBefore(m_x509));
    m_info.not_after  = asn1_time_to_unix(X509_get0_notAfter(m_x509));
    parse_sans();
    parse_key_usage();
}
#endif

#if defined(OS_LINUX)
std::string OpenSSLCertificate::name_to_string(X509_NAME* name) noexcept {
    if (!name) return {};
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) return {};
    X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253);
    char* data = nullptr;
    long len = BIO_get_mem_data(bio, &data);
    std::string out = (len > 0 && data) ? std::string(data, static_cast<std::size_t>(len)) : std::string();
    BIO_free(bio);
    return out;
}
#endif

#if defined(OS_LINUX)
std::string OpenSSLCertificate::serial_to_string() const noexcept {
    const ASN1_INTEGER* serial = X509_get0_serialNumber(m_x509);
    if (!serial || serial->length <= 0) return {};
    std::string out;
    out.reserve(static_cast<std::size_t>(serial->length) * 3);

    for (int i = 0; i < serial->length; ++i) {
        char hex[4];
        std::snprintf(hex, sizeof(hex), "%02X", serial->data[i]);
        if (!out.empty()) out += ':';
        out += hex;
    }

    return out;
}
#endif

#if defined(OS_LINUX)
std::int64_t OpenSSLCertificate::asn1_time_to_unix(const ASN1_TIME* t) noexcept {
    if (!t) return 0;
    struct tm tm_value;
    std::memset(&tm_value, 0, sizeof(tm_value));
    if (ASN1_TIME_to_tm(t, &tm_value) != 1) return 0;
    return static_cast<std::int64_t>(timegm(&tm_value));
}
#endif

#if defined(OS_LINUX)
void OpenSSLCertificate::parse_sans() noexcept {
    auto* names = static_cast<GENERAL_NAMES*>(
        X509_get_ext_d2i(m_x509, NID_subject_alt_name, nullptr, nullptr)
    );

    if (!names) return;
    const int count = sk_GENERAL_NAME_num(names);

    for (int i = 0; i < count; ++i) {
        const GENERAL_NAME* gn = sk_GENERAL_NAME_value(names, i);
        if (!gn) continue;

        if (gn->type == GEN_DNS) {
            const unsigned char* data = ASN1_STRING_get0_data(gn->d.dNSName);
            int len = ASN1_STRING_length(gn->d.dNSName);

            if (data && len > 0) {
                m_info.sans.push_back({
                    SubjectAltName::Type::DNS,
                    std::string(reinterpret_cast<const char*>(data), static_cast<std::size_t>(len))
                });
            }
        } else if (gn->type == GEN_IPADD) {
            const unsigned char* data = ASN1_STRING_get0_data(gn->d.iPAddress);
            int len = ASN1_STRING_length(gn->d.iPAddress);

            if (data && len == 4) {
                char addr[INET_ADDRSTRLEN]{};
                ::inet_ntop(AF_INET, data, addr, sizeof(addr));
                m_info.sans.push_back({ SubjectAltName::Type::IP, addr });
            } else if (data && len == 16) {
                char addr[INET6_ADDRSTRLEN]{};
                ::inet_ntop(AF_INET6, data, addr, sizeof(addr));
                m_info.sans.push_back({ SubjectAltName::Type::IP, addr });
            }
        }
    }

    GENERAL_NAMES_free(names);
}
#endif

#if defined(OS_LINUX)
void OpenSSLCertificate::parse_key_usage() noexcept {
    auto* bits = static_cast<ASN1_BIT_STRING*>(
        X509_get_ext_d2i(m_x509, NID_key_usage, nullptr, nullptr)
    );

    if (!bits) return;
    auto bit = [bits](int index) noexcept { return ASN1_BIT_STRING_get_bit(bits, index) != 0; };
    m_info.key_usage.digital_signature = bit(0);
    m_info.key_usage.non_repudiation   = bit(1);
    m_info.key_usage.key_encipherment  = bit(2);
    m_info.key_usage.data_encipherment = bit(3);
    m_info.key_usage.key_agreement     = bit(4);
    m_info.key_usage.cert_sign         = bit(5);
    m_info.key_usage.crl_sign          = bit(6);
    m_info.key_usage.encipher_only     = bit(7);
    m_info.key_usage.decipher_only     = bit(8);
    ASN1_BIT_STRING_free(bits);
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCertOps::issuer_links_to_subject(
    const NativeCertificateBase& child,
    const NativeCertificateBase& issuer
) noexcept {
    auto c = static_cast<X509*>(child.native());
    auto i = static_cast<X509*>(issuer.native());
    if (!c || !i) return false;
    return X509_NAME_cmp(X509_get_issuer_name(c), X509_get_subject_name(i)) == 0;
}
#endif

#if defined(OS_LINUX)
auto OpenSSLCertOps::verify_chain(
    const NativeCertificateBase& leaf,
    const std::vector<const NativeCertificateBase*>& extras,
    const std::string& hostname,
    const std::string& ca_store_path,
    bool check_revocation
) noexcept -> VerifyResult {
    openssl_init_once();
    auto leaf_x509 = static_cast<X509*>(leaf.native());
    if (!leaf_x509) { return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "No leaf certificate"); }
    X509_STORE* store = X509_STORE_new();
    if (!store) { return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "X509_STORE_new failed"); }

    if (ca_store_path.empty()) {
        X509_STORE_set_default_paths(store);
    } else if (!X509_STORE_load_locations(store, ca_store_path.c_str(), nullptr)) {
        openssl_drain_errors();

        if (!X509_STORE_load_locations(store, nullptr, ca_store_path.c_str())) {
            openssl_drain_errors();
            X509_STORE_free(store);
            return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "Could not load CA store \"" + ca_store_path + "\"");
        }
    }

    STACK_OF(X509)* untrusted = sk_X509_new_null();

    if (untrusted) {
        for (const auto* cert : extras) {
            if (!cert) continue;
            auto x = static_cast<X509*>(cert->native());
            if (x) sk_X509_push(untrusted, x);
        }
    }

    X509_STORE_CTX* ctx = X509_STORE_CTX_new();

    if (!ctx || X509_STORE_CTX_init(ctx, store, leaf_x509, untrusted) != 1) {
        if (ctx) X509_STORE_CTX_free(ctx);
        if (untrusted) sk_X509_free(untrusted);
        X509_STORE_free(store);
        return VerifyResult::failure(VerifyError::UntrustedRoot, 0, "X509_STORE_CTX_init failed");
    }

    X509_VERIFY_PARAM* param = X509_STORE_CTX_get0_param(ctx);

    if (param) {
        if (!hostname.empty()) {
            X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
            X509_VERIFY_PARAM_set1_host(param, hostname.c_str(), hostname.size());
        }

        if (check_revocation && !ca_store_path.empty()) {
            X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
        }
    }

    const int ok    = X509_verify_cert(ctx);
    const int error = X509_STORE_CTX_get_error(ctx);
    const int depth = X509_STORE_CTX_get_error_depth(ctx);
    X509_STORE_CTX_free(ctx);
    if (untrusted) sk_X509_free(untrusted);
    X509_STORE_free(store);
    openssl_drain_errors();
    if (ok == 1) return VerifyResult::success();
    return map_verify_error(error, depth);
}
#endif

#if defined(OS_LINUX)
auto OpenSSLCertOps::map_verify_error(int error, int depth) noexcept -> VerifyResult {
    switch (error) {
        case X509_V_ERR_CERT_HAS_EXPIRED:
        case X509_V_ERR_CERT_NOT_YET_VALID:
        case X509_V_ERR_CRL_HAS_EXPIRED:
        case X509_V_ERR_CRL_NOT_YET_VALID:
            return VerifyResult::failure(VerifyError::Expired, depth, X509_verify_cert_error_string(error));

        case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
            return VerifyResult::failure(VerifyError::SelfSigned, depth, X509_verify_cert_error_string(error));

        case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        case X509_V_ERR_CERT_UNTRUSTED:
            return VerifyResult::failure(VerifyError::UntrustedRoot, depth, X509_verify_cert_error_string(error));

        case X509_V_ERR_HOSTNAME_MISMATCH:
        case X509_V_ERR_IP_ADDRESS_MISMATCH:
            return VerifyResult::failure(VerifyError::HostnameMismatch, depth, X509_verify_cert_error_string(error));

        case X509_V_ERR_CERT_REVOKED:
            return VerifyResult::failure(VerifyError::Revoked, depth, X509_verify_cert_error_string(error));

        case X509_V_ERR_UNABLE_TO_GET_CRL:
        case X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER:
            return VerifyResult::failure(VerifyError::RevocationUnknown, depth, X509_verify_cert_error_string(error));

        case X509_V_ERR_CERT_SIGNATURE_FAILURE:
        case X509_V_ERR_CRL_SIGNATURE_FAILURE:
        case X509_V_ERR_UNSUPPORTED_SIGNATURE_ALGORITHM:
        case X509_V_ERR_INVALID_PURPOSE:
        case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
            return VerifyResult::failure(VerifyError::WeakSignature, depth, X509_verify_cert_error_string(error));

        default:
            return VerifyResult::failure(
                VerifyError::UntrustedRoot, depth,
                std::string("X509 error ") + std::to_string(error) + ": " + X509_verify_cert_error_string(error)
            );
    }
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCredentials::acquire(const TLSConfig& config) noexcept {
    release();
    m_ctx = SSL_CTX_new(TLS_method());

    if (!m_ctx) {
        m_last_error = "SSL_CTX_new failed: " + openssl_last_error();
        return false;
    }

    SSL_CTX_set_min_proto_version(m_ctx, static_cast<int>(config.min_version));
    SSL_CTX_set_max_proto_version(m_ctx, static_cast<int>(config.max_version));
    SSL_CTX_set_options(m_ctx, SSL_OP_NO_COMPRESSION | SSL_OP_CIPHER_SERVER_PREFERENCE);
    SSL_CTX_set_mode(m_ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_ENABLE_PARTIAL_WRITE);
    if (!apply_cipher_suites(config)) { release(); return false; }
    if (!apply_trust_store(config))   { release(); return false; }
    if (!apply_certificate(config))   { release(); return false; }
    int mode = SSL_VERIFY_NONE;

    if (config.verify_mode == VerifyMode::FailIfNoPeer) {
        mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    } else if (config.verify_mode == VerifyMode::Peer && config.use == ContextUse::Server) {
        mode = SSL_VERIFY_PEER;
    }

    SSL_CTX_set_verify(m_ctx, mode, nullptr);
    return true;
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCredentials::apply_cipher_suites(const TLSConfig& config) noexcept {
    if (config.cipher_suites.empty()) return true;
    std::string list;

    for (const auto& suite : config.cipher_suites) {
        if (!list.empty()) list += ':';
        list += suite;
    }

    const bool legacy_ok = SSL_CTX_set_cipher_list(m_ctx, list.c_str()) == 1;
    openssl_drain_errors();
    const bool tls13_ok  = SSL_CTX_set_ciphersuites(m_ctx, list.c_str()) == 1;
    openssl_drain_errors();

    if (!legacy_ok && !tls13_ok) {
        m_last_error = "No configured cipher suite is recognised";
        return false;
    }

    return true;
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCredentials::apply_trust_store(const TLSConfig& config) noexcept {
    if (config.ca_store_path.empty()) {
        SSL_CTX_set_default_verify_paths(m_ctx);
        return true;
    }

    if (SSL_CTX_load_verify_locations(m_ctx, config.ca_store_path.c_str(), nullptr) == 1) return true;
    openssl_drain_errors();
    if (SSL_CTX_load_verify_locations(m_ctx, nullptr, config.ca_store_path.c_str()) == 1) return true;
    m_last_error = "Could not load CA store \"" + config.ca_store_path + "\": " + openssl_last_error();
    return false;
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCredentials::apply_certificate(const TLSConfig& config) noexcept {
    if (config.certificate.empty()) return true;
    if (load_pkcs12(config)) return true;
    return load_der_pair(config);
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCredentials::load_pkcs12(const TLSConfig& config) noexcept {
    BIO* bio = BIO_new_mem_buf(config.certificate.chain.data(), static_cast<int>(config.certificate.chain.size()));
    if (!bio) return false;
    PKCS12* p12 = d2i_PKCS12_bio(bio, nullptr);
    BIO_free(bio);
    if (!p12) { openssl_drain_errors(); return false; }
    EVP_PKEY*       key   = nullptr;
    X509*           cert  = nullptr;
    STACK_OF(X509)* extra = nullptr;
    const int ok = PKCS12_parse(p12, config.certificate.password.c_str(), &key, &cert, &extra);
    PKCS12_free(p12);

    if (ok != 1 || !cert || !key) {
        if (key) EVP_PKEY_free(key);
        if (cert) X509_free(cert);
        if (extra) sk_X509_pop_free(extra, X509_free);
        openssl_drain_errors();
        return false;
    }

    bool result = SSL_CTX_use_certificate(m_ctx, cert) == 1 && SSL_CTX_use_PrivateKey(m_ctx, key) == 1;

    if (result && extra) {
        for (int i = 0; i < sk_X509_num(extra); ++i) {
            X509* ca = sk_X509_value(extra, i);
            if (!ca) continue;
            X509_up_ref(ca);
            if (SSL_CTX_add_extra_chain_cert(m_ctx, ca) != 1) { X509_free(ca); }
        }
    }

    if (result && SSL_CTX_check_private_key(m_ctx) != 1) {
        m_last_error = "Private key does not match the certificate";
        result = false;
    }

    EVP_PKEY_free(key);
    X509_free(cert);
    if (extra) sk_X509_pop_free(extra, X509_free);
    openssl_drain_errors();
    return result;
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCredentials::load_der_pair(const TLSConfig& config) noexcept {
    const auto& chain = config.certificate.chain;

    if (SSL_CTX_use_certificate_ASN1(m_ctx, static_cast<int>(chain.size()), chain.data()) != 1) {
        m_last_error = "Certificate blob is neither a PKCS#12 nor a DER certificate: " + openssl_last_error();
        return false;
    }

    if (config.certificate.private_key.empty()) {
        m_last_error = "A DER certificate was supplied without a private key";
        return false;
    }

    const unsigned char* p = config.certificate.private_key.data();
    EVP_PKEY* key = d2i_AutoPrivateKey(nullptr, &p, static_cast<long>(config.certificate.private_key.size()));

    if (!key) {
        m_last_error = "Private key blob could not be parsed: " + openssl_last_error();
        return false;
    }

    bool result = SSL_CTX_use_PrivateKey(m_ctx, key) == 1 && SSL_CTX_check_private_key(m_ctx) == 1;
    if (!result) { m_last_error = "Private key does not match the certificate"; }
    EVP_PKEY_free(key);
    openssl_drain_errors();
    return result;
}
#endif

#if defined(OS_LINUX)
bool OpenSSLSession::open(NativeCredentialsBase& credentials, bool server, const std::string& hostname) noexcept {
    close_session();
    auto ctx = static_cast<SSL_CTX*>(credentials.native());
    if (!ctx) return false;
    m_ssl = SSL_new(ctx);
    if (!m_ssl) { openssl_drain_errors(); return false; }
    m_rbio = BIO_new(BIO_s_mem());
    m_wbio = BIO_new(BIO_s_mem());

    if (!m_rbio || !m_wbio) {
        if (m_rbio) BIO_free(m_rbio);
        if (m_wbio) BIO_free(m_wbio);
        m_rbio = m_wbio = nullptr;
        SSL_free(m_ssl);
        m_ssl = nullptr;
        return false;
    }

    BIO_set_mem_eof_return(m_rbio, -1);
    BIO_set_mem_eof_return(m_wbio, -1);
    SSL_set_bio(m_ssl, m_rbio, m_wbio);   
    m_server   = server;
    m_hostname = hostname;

    if (server) {
        SSL_set_accept_state(m_ssl);
    } else {
        SSL_set_connect_state(m_ssl);

        if (!hostname.empty()) {
            SSL_set_tlsext_host_name(m_ssl, hostname.c_str());
            SSL_set1_host(m_ssl, hostname.c_str());
        }
    }

    return true;
}
#endif

#if defined(OS_LINUX)
auto OpenSSLSession::handshake(const std::uint8_t* input, std::size_t length) noexcept -> HandshakeIO {
    HandshakeIO io;

    if (!m_ssl) {
        io.status = HandshakeStatus::Failed;
        io.detail = "Session not open";
        return io;
    }

    if (input && length > 0) {
        if (BIO_write(m_rbio, input, static_cast<int>(length)) <= 0) {
            io.status = HandshakeStatus::Failed;
            io.detail = "Could not buffer incoming handshake data";
            return io;
        }
    }

       
    if (m_server && (!input || length == 0) && BIO_pending(m_rbio) == 0) {
        io.status = HandshakeStatus::NeedMoreData;
        return io;
    }

    ERR_clear_error();
    const int result = SSL_do_handshake(m_ssl);
    drain_wbio(io.outgoing);

    if (result == 1) {
        m_done = true;
        io.status = HandshakeStatus::Complete;
        return io;
    }

    const int reason = SSL_get_error(m_ssl, result);
    io.native_code = reason;

    switch (reason) {
        case SSL_ERROR_WANT_READ:
            io.status = io.outgoing.empty() ? HandshakeStatus::NeedMoreData : HandshakeStatus::Continue;
            return io;

        case SSL_ERROR_WANT_WRITE:
            io.status = HandshakeStatus::Continue;
            return io;

        case SSL_ERROR_ZERO_RETURN:
            io.status = HandshakeStatus::Failed;
            io.detail = "Peer closed the connection during the handshake";
            return io;

        default:
            io.status = HandshakeStatus::Failed;
            io.detail = describe_failure(reason);
            return io;
    }
}
#endif

#if defined(OS_LINUX)
int OpenSSLSession::encrypt(const std::uint8_t* plaintext, std::size_t length, std::vector<std::uint8_t>& out) noexcept {
    if (!m_ssl || !m_done || !plaintext || length == 0) return -1;
    std::size_t sent = 0;

    while (sent < length) {
        const std::size_t chunk = std::min<std::size_t>(length - sent, max_plaintext_chunk());
        ERR_clear_error();
        const int written = SSL_write(m_ssl, plaintext + sent, static_cast<int>(chunk));
        drain_wbio(out);

        if (written <= 0) {
            const int reason = SSL_get_error(m_ssl, written);
            openssl_drain_errors();
            if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE) break;
            return sent > 0 ? static_cast<int>(sent) : -1;
        }

        sent += static_cast<std::size_t>(written);
    }

    return static_cast<int>(sent);
}
#endif

#if defined(OS_LINUX)
auto OpenSSLSession::decrypt(const std::uint8_t* ciphertext, std::size_t length) noexcept -> DecryptIO {
    DecryptIO io;
    if (!m_ssl || !m_done) { io.status = IOStatus::Failed; return io; }

    if (ciphertext && length > 0) {
        if (BIO_write(m_rbio, ciphertext, static_cast<int>(length)) <= 0) {
            io.status = IOStatus::Failed;
            return io;
        }
    }

    std::uint8_t buffer[16384];
    ERR_clear_error();
    const int read = SSL_read(m_ssl, buffer, static_cast<int>(sizeof(buffer)));
    drain_wbio(io.outgoing); 

    if (read > 0) {
        io.status = IOStatus::Ok;
        io.plaintext.assign(buffer, buffer + read);
        return io;
    }

    const int reason = SSL_get_error(m_ssl, read);
    io.native_code = reason;

    switch (reason) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            io.status = IOStatus::NeedMoreData;
            return io;

        case SSL_ERROR_ZERO_RETURN:
            io.status = IOStatus::Closed;
            return io;

        default:
            io.status = IOStatus::Failed;
            openssl_drain_errors();
            return io;
    }
}
#endif

#if defined(OS_LINUX)
bool OpenSSLSession::close_notify(std::vector<std::uint8_t>& out) noexcept {
    if (!m_ssl || !m_done) return false;
    ERR_clear_error();
    SSL_shutdown(m_ssl);
    openssl_drain_errors();
    drain_wbio(out);
    return !out.empty();
}
#endif

#if defined(OS_LINUX)
bool OpenSSLSession::peer_chain(std::vector<std::vector<std::uint8_t>>& out) const noexcept {
    if (!m_ssl) return false;
    X509* leaf = FIZMO_SSL_GET1_PEER_CERT(m_ssl);
    if (!leaf) return false;
    if (!append_der(leaf, out)) { X509_free(leaf); return false; }
    STACK_OF(X509)* chain = SSL_get_peer_cert_chain(m_ssl);

    if (chain) {
        for (int i = 0; i < sk_X509_num(chain); ++i) {
            X509* cert = sk_X509_value(chain, i);
            if (!cert || X509_cmp(cert, leaf) == 0) continue;
            append_der(cert, out);
        }
    }

    X509_free(leaf);
    return true;
}
#endif

#if defined(OS_LINUX)
std::string OpenSSLSession::cipher_name() const noexcept {
    if (!m_ssl || !m_done) return {};
    const char* name = SSL_get_cipher_name(m_ssl);
    return name ? std::string(name) : std::string();
}
#endif

#if defined(OS_LINUX)
auto OpenSSLSession::protocol_version() const noexcept -> TLSVersion {
    if (!m_ssl || !m_done) return TLSVersion::TLS_1_2;

    switch (SSL_version(m_ssl)) {
        case TLS1_VERSION:   return TLSVersion::TLS_1_0;
        case TLS1_1_VERSION: return TLSVersion::TLS_1_1;
        case TLS1_2_VERSION: return TLSVersion::TLS_1_2;
    #ifdef TLS1_3_VERSION
        case TLS1_3_VERSION: return TLSVersion::TLS_1_3;
    #endif
        default:             return TLSVersion::TLS_1_2;
    }
}
#endif

#if defined(OS_LINUX)
void OpenSSLSession::drain_wbio(std::vector<std::uint8_t>& out) noexcept {
    if (!m_wbio) return;

    for (;;) {
        const int pending = BIO_pending(m_wbio);
        if (pending <= 0) return;
        const std::size_t offset = out.size();
        out.resize(offset + static_cast<std::size_t>(pending));
        const int read = BIO_read(m_wbio, out.data() + offset, pending);

        if (read <= 0) {
            out.resize(offset);
            return;
        }

        out.resize(offset + static_cast<std::size_t>(read));
    }
}
#endif

#if defined(OS_LINUX)
bool OpenSSLSession::append_der(X509* cert, std::vector<std::vector<std::uint8_t>>& out) noexcept {
    unsigned char* der = nullptr;
    const int len = i2d_X509(cert, &der);
    if (len <= 0 || !der) { openssl_drain_errors(); return false; }
    out.emplace_back(der, der + len);
    OPENSSL_free(der);
    return true;
}
#endif

#if defined(OS_LINUX)
std::string OpenSSLSession::describe_failure(int reason) noexcept {
    std::string text = openssl_last_error();
    if (!text.empty()) return text;
    return "SSL error " + std::to_string(reason);
}
#endif

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo
