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
    bool check_revocation,
    const std::vector<std::string>& crl_paths
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

    bool has_crls = false;

    if (!crl_paths.empty()) {
        const int n = load_crls(store, crl_paths);

        if (n < 0) {
            X509_STORE_free(store);
            return VerifyResult::failure(VerifyError::RevocationUnknown, 0, "Could not load CRL files");
        }

        has_crls = n > 0;
    }

    if (!has_crls) {
        STACK_OF(X509_OBJECT)* objs = X509_STORE_get0_objects(store);

        for (int i = 0; objs && i < sk_X509_OBJECT_num(objs); ++i) {
            if (X509_OBJECT_get_type(sk_X509_OBJECT_value(objs, i)) == X509_LU_CRL) { has_crls = true; break; }
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
            if (X509_VERIFY_PARAM_set1_ip_asc(param, hostname.c_str()) != 1) {
                openssl_drain_errors();
                X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
                X509_VERIFY_PARAM_set1_host(param, hostname.c_str(), hostname.size());
            }
        }

        if (check_revocation && has_crls) {
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

    if (!config.allow_insecure_versions && static_cast<std::uint32_t>(config.min_version) < static_cast<std::uint32_t>(TLSVersion::TLS_1_2)) {
        m_last_error = "Insecure minimum protocol version requested (below TLS 1.2); set allow_insecure_versions to override";
        return false;
    }

    if (static_cast<std::uint32_t>(config.min_version) > static_cast<std::uint32_t>(config.max_version)) {
        m_last_error = "Minimum protocol version is greater than the maximum";
        return false;
    }

    try { m_config = config; } catch (...) { m_last_error = "Out of memory"; return false; }
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
    if (!apply_extensions(config)) { release(); return false; }
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

    std::string legacy_list, tls13_list;

    for (const auto& suite : config.cipher_suites) {
        const bool is13 = suite.rfind("TLS_AES_", 0) == 0 || suite.rfind("TLS_CHACHA20_", 0) == 0;
        std::string& target = is13 ? tls13_list : legacy_list;
        if (!target.empty()) target += ':';
        target += suite;
    }

    (void)list;
    bool legacy_ok = false, tls13_ok = false;

    if (!legacy_list.empty()) {
        legacy_ok = SSL_CTX_set_cipher_list(m_ctx, legacy_list.c_str()) == 1;
        openssl_drain_errors();
    }

    if (!tls13_list.empty()) {
        tls13_ok = SSL_CTX_set_ciphersuites(m_ctx, tls13_list.c_str()) == 1;
        openssl_drain_errors();
    }

    if (!legacy_ok && !tls13_ok) {
        m_last_error = "No configured cipher suite is recognised";
        return false;
    }

    return true;
}
#endif

#if defined(OS_LINUX)
bool OpenSSLCredentials::apply_trust_store(const TLSConfig& config) noexcept {
    bool loaded = false;

    if (config.ca_store_path.empty()) {
        SSL_CTX_set_default_verify_paths(m_ctx);
        loaded = true;
    } else if (SSL_CTX_load_verify_locations(m_ctx, config.ca_store_path.c_str(), nullptr) == 1) {
        loaded = true;
    } else {
        openssl_drain_errors();
        if (SSL_CTX_load_verify_locations(m_ctx, nullptr, config.ca_store_path.c_str()) == 1) loaded = true;
    }

    if (!loaded) {
        m_last_error = "Could not load CA store \"" + config.ca_store_path + "\": " + openssl_last_error();
        return false;
    }

    if (!config.crl_paths.empty()) {
        X509_STORE* store = SSL_CTX_get_cert_store(m_ctx);
        const int n = OpenSSLCertOps::load_crls(store, config.crl_paths);

        if (n < 0) {
            m_last_error = "Could not load a CRL file";
            return false;
        }

        if (n > 0 && config.check_revocation) X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    }

    return true;
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
    m_alert    = AlertInfo();
    SSL_set_app_data(m_ssl, this);
    SSL_set_info_callback(m_ssl, [](const SSL* s, int where, int ret) {
        if ((where & SSL_CB_ALERT) == 0) return;
        auto* self = static_cast<OpenSSLSession*>(SSL_get_app_data(s));
        if (self) self->record_alert((where & SSL_CB_READ) != 0, ret);
    });
    const TLSConfig& cfg = static_cast<OpenSSLCredentials&>(credentials).config();

    if (!server) {
        if (cfg.request_ocsp) SSL_set_tlsext_status_type(m_ssl, TLSEXT_STATUSTYPE_ocsp);
    #if OPENSSL_VERSION_NUMBER >= 0x10101000L
        if (cfg.post_handshake_auth) SSL_set_post_handshake_auth(m_ssl, 1);
    #endif
    }

    if (server) {
        SSL_set_accept_state(m_ssl);
    } else {
        SSL_set_connect_state(m_ssl);

        unsigned char ipbuf[16];
        const bool is_ip = inet_pton(AF_INET, hostname.c_str(), ipbuf) == 1 || inet_pton(AF_INET6, hostname.c_str(), ipbuf) == 1;

        if (!hostname.empty() && !is_ip) {
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

#if defined(OS_LINUX)
bool OpenSSLCredentials::keylog_supported() noexcept {
#if defined(FIZMO_TLS_KEYLOG_ENABLE) && OPENSSL_VERSION_NUMBER >= 0x10101000L
    return true;
#else
    return false;
#endif
}

void OpenSSLCredentials::keylog_callback(const SSL* ssl, const char* line) {
    SSL_CTX* ctx = SSL_get_SSL_CTX(ssl);
    auto* self = ctx ? static_cast<OpenSSLCredentials*>(SSL_CTX_get_app_data(ctx)) : nullptr;
    if (!self || !line) return;
    std::lock_guard<std::mutex> lock(self->m_keylog_mutex);

    if (self->m_config.keylog_callback) {
        try { self->m_config.keylog_callback(line); } catch (...) {}
    }

    if (!self->m_config.keylog_path.empty()) {
        FILE* f = std::fopen(self->m_config.keylog_path.c_str(), "a");

        if (f) {
            std::fputs(line, f);
            std::fputc('\n', f);
            std::fclose(f);
        }
    }
}

int OpenSSLCredentials::alpn_select(SSL*, const unsigned char** out, unsigned char* outlen, const unsigned char* in, unsigned int inlen, void* arg) {
    auto* self = static_cast<OpenSSLCredentials*>(arg);
    if (!self) return SSL_TLSEXT_ERR_NOACK;

    for (const auto& want : self->m_config.alpn_protocols) {
        unsigned int i = 0;

        while (i < inlen) {
            const unsigned int len = in[i];
            if (i + 1 + len > inlen) break;

            if (len == want.size() && std::memcmp(in + i + 1, want.data(), len) == 0) {
                *out = in + i + 1;
                *outlen = static_cast<unsigned char>(len);
                return SSL_TLSEXT_ERR_OK;
            }

            i += 1 + len;
        }
    }

    return SSL_TLSEXT_ERR_ALERT_FATAL;
}

int OpenSSLCredentials::new_session(SSL* ssl, SSL_SESSION* session) {
    auto* owner = static_cast<OpenSSLSession*>(SSL_get_app_data(ssl));
    if (owner) owner->deliver_session(session);
    return 0;
}

int OpenSSLCredentials::status_callback(SSL* ssl, void* arg) {
    auto* self = static_cast<OpenSSLCredentials*>(arg);
    if (!self || self->m_config.ocsp_staple.empty()) return SSL_TLSEXT_ERR_NOACK;
    const auto& staple = self->m_config.ocsp_staple;
    auto* copy = static_cast<unsigned char*>(OPENSSL_malloc(staple.size()));
    if (!copy) return SSL_TLSEXT_ERR_NOACK;
    std::memcpy(copy, staple.data(), staple.size());

    if (SSL_set_tlsext_status_ocsp_resp(ssl, copy, static_cast<long>(staple.size())) != 1) {
        OPENSSL_free(copy);
        return SSL_TLSEXT_ERR_NOACK;
    }

    return SSL_TLSEXT_ERR_OK;
}

bool OpenSSLCredentials::apply_extensions(const TLSConfig& config) noexcept {
    SSL_CTX_set_app_data(m_ctx, this);
    const bool server = config.use == ContextUse::Server;

    if (!config.alpn_protocols.empty()) {
        m_alpn_wire.clear();

        for (const auto& p : config.alpn_protocols) {
            if (p.empty() || p.size() > 255) {
                m_last_error = "ALPN protocol names must be 1 to 255 bytes long";
                return false;
            }

            m_alpn_wire.push_back(static_cast<std::uint8_t>(p.size()));
            m_alpn_wire.insert(m_alpn_wire.end(), p.begin(), p.end());
        }

        if (server) {
            SSL_CTX_set_alpn_select_cb(m_ctx, &OpenSSLCredentials::alpn_select, this);
        } else if (SSL_CTX_set_alpn_protos(m_ctx, m_alpn_wire.data(), static_cast<unsigned int>(m_alpn_wire.size())) != 0) {
            m_last_error = "Could not configure ALPN";
            return false;
        }
    }

    if (!config.session_tickets) SSL_CTX_set_options(m_ctx, SSL_OP_NO_TICKET);
    if (config.session_timeout_seconds > 0) SSL_CTX_set_timeout(m_ctx, static_cast<long>(config.session_timeout_seconds));

    if (server) {
        static const unsigned char sid_ctx[] = "fizmo";
        SSL_CTX_set_session_cache_mode(m_ctx, SSL_SESS_CACHE_SERVER);
        SSL_CTX_set_session_id_context(m_ctx, sid_ctx, sizeof(sid_ctx) - 1);

        if (!config.ocsp_staple.empty()) {
            SSL_CTX_set_tlsext_status_cb(m_ctx, &OpenSSLCredentials::status_callback);
            SSL_CTX_set_tlsext_status_arg(m_ctx, this);
        }
    } else {
        SSL_CTX_set_session_cache_mode(m_ctx, SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_STORE);
        SSL_CTX_sess_set_new_cb(m_ctx, &OpenSSLCredentials::new_session);
    }

#if defined(SSL_OP_NO_RENEGOTIATION)
    if (!config.allow_renegotiation) SSL_CTX_set_options(m_ctx, SSL_OP_NO_RENEGOTIATION);
#endif

#if defined(FIZMO_TLS_KEYLOG_ENABLE) && OPENSSL_VERSION_NUMBER >= 0x10101000L
    if (!config.keylog_path.empty() || config.keylog_callback) SSL_CTX_set_keylog_callback(m_ctx, &OpenSSLCredentials::keylog_callback);
#endif

    return true;
}

int OpenSSLCertOps::load_crls(X509_STORE* store, const std::vector<std::string>& paths) noexcept {
    int count = 0;

    for (const auto& path : paths) {
        BIO* bio = BIO_new_file(path.c_str(), "rb");
        if (!bio) { openssl_drain_errors(); return -1; }
        int here = 0;

        for (;;) {
            X509_CRL* crl = PEM_read_bio_X509_CRL(bio, nullptr, nullptr, nullptr);
            if (!crl) break;
            if (X509_STORE_add_crl(store, crl) == 1) ++here;
            X509_CRL_free(crl);
        }

        openssl_drain_errors();

        if (here == 0) {
            BIO_reset(bio);
            X509_CRL* crl = d2i_X509_CRL_bio(bio, nullptr);

            if (crl) {
                if (X509_STORE_add_crl(store, crl) == 1) ++here;
                X509_CRL_free(crl);
            }

            openssl_drain_errors();
        }

        BIO_free(bio);
        if (here == 0) return -1;
        count += here;
    }

    return count;
}

namespace {

std::int64_t asn1_to_unix(const ASN1_TIME* t) noexcept {
    if (!t) return 0;
    std::tm tm{};
    if (ASN1_TIME_to_tm(t, &tm) != 1) return 0;
    return static_cast<std::int64_t>(timegm(&tm));
}

X509_STORE* make_store(const std::string& ca_store_path) noexcept {
    X509_STORE* store = X509_STORE_new();
    if (!store) return nullptr;

    if (ca_store_path.empty()) {
        X509_STORE_set_default_paths(store);
    } else if (!X509_STORE_load_locations(store, ca_store_path.c_str(), nullptr)) {
        openssl_drain_errors();

        if (!X509_STORE_load_locations(store, nullptr, ca_store_path.c_str())) {
            openssl_drain_errors();
            X509_STORE_free(store);
            return nullptr;
        }
    }

    return store;
}

} // namespace

OCSPResult OpenSSLCertOps::verify_ocsp(
    const std::vector<std::uint8_t>& response,
    const NativeCertificateBase& leaf,
    const NativeCertificateBase* issuer,
    const std::string& ca_store_path
) noexcept {
    openssl_init_once();
    OCSPResult result;
    auto leaf_x = static_cast<X509*>(leaf.native());
    auto issuer_x = issuer ? static_cast<X509*>(issuer->native()) : nullptr;

    if (response.empty()) { result.status = OCSPCertStatus::Invalid; result.detail = "Empty OCSP response"; return result; }
    if (!leaf_x || !issuer_x) { result.status = OCSPCertStatus::Invalid; result.detail = "Leaf and issuer certificates are required"; return result; }
    const unsigned char* p = response.data();
    OCSP_RESPONSE* resp = d2i_OCSP_RESPONSE(nullptr, &p, static_cast<long>(response.size()));
    if (!resp) { openssl_drain_errors(); result.status = OCSPCertStatus::Invalid; result.detail = "Malformed OCSP response"; return result; }

    if (OCSP_response_status(resp) != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
        OCSP_RESPONSE_free(resp);
        result.status = OCSPCertStatus::Invalid;
        result.detail = "OCSP responder returned an error status";
        return result;
    }

    OCSP_BASICRESP* basic = OCSP_response_get1_basic(resp);
    OCSP_RESPONSE_free(resp);
    if (!basic) { openssl_drain_errors(); result.status = OCSPCertStatus::Invalid; result.detail = "OCSP response has no basic response"; return result; }
    X509_STORE* store = make_store(ca_store_path);
    STACK_OF(X509)* certs = sk_X509_new_null();
    if (certs) sk_X509_push(certs, issuer_x);
    const int verified = store ? OCSP_basic_verify(basic, certs, store, 0) : 0;
    if (certs) sk_X509_free(certs);
    if (store) X509_STORE_free(store);

    if (verified <= 0) {
        openssl_drain_errors();
        OCSP_BASICRESP_free(basic);
        result.status = OCSPCertStatus::Invalid;
        result.detail = "OCSP response signature could not be verified";
        return result;
    }

    OCSP_CERTID* id = OCSP_cert_to_id(nullptr, leaf_x, issuer_x);
    int status = -1, reason = -1;
    ASN1_GENERALIZEDTIME *rev = nullptr, *thisupd = nullptr, *nextupd = nullptr;

    if (!id || OCSP_resp_find_status(basic, id, &status, &reason, &rev, &thisupd, &nextupd) != 1) {
        if (id) OCSP_CERTID_free(id);
        OCSP_BASICRESP_free(basic);
        openssl_drain_errors();
        result.status = OCSPCertStatus::Unknown;
        result.detail = "OCSP response does not cover this certificate";
        return result;
    }

    result.this_update = asn1_to_unix(thisupd);
    result.next_update = asn1_to_unix(nextupd);
    result.revocation_time = asn1_to_unix(rev);
    result.revocation_reason = reason;

    if (OCSP_check_validity(thisupd, nextupd, 300, -1) != 1) {
        result.status = OCSPCertStatus::Invalid;
        result.detail = "OCSP response is outside its validity window";
    } else if (status == V_OCSP_CERTSTATUS_GOOD) {
        result.status = OCSPCertStatus::Good;
    } else if (status == V_OCSP_CERTSTATUS_REVOKED) {
        result.status = OCSPCertStatus::Revoked;
        result.detail = "Certificate has been revoked";
    } else {
        result.status = OCSPCertStatus::Unknown;
        result.detail = "Responder does not know this certificate";
    }

    OCSP_CERTID_free(id);
    OCSP_BASICRESP_free(basic);
    openssl_drain_errors();
    return result;
}

bool OpenSSLCertOps::load_system_roots(std::vector<std::vector<std::uint8_t>>& out, const std::string& store_name) noexcept {
    (void)store_name;
    static const char* const bundles[] = {
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/ca-bundle.pem",
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
        "/etc/ssl/cert.pem"
    };

    const char* env = std::getenv("SSL_CERT_FILE");
    std::vector<std::string> candidates;
    if (env && *env) candidates.push_back(env);
    for (const char* b : bundles) candidates.push_back(b);

    for (const auto& path : candidates) {
        BIO* bio = BIO_new_file(path.c_str(), "rb");
        if (!bio) { openssl_drain_errors(); continue; }
        std::size_t before = out.size();

        for (;;) {
            X509* x = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
            if (!x) break;
            unsigned char* der = nullptr;
            const int len = i2d_X509(x, &der);
            if (len > 0 && der) out.emplace_back(der, der + len);
            if (der) OPENSSL_free(der);
            X509_free(x);
        }

        BIO_free(bio);
        openssl_drain_errors();
        if (out.size() > before) return true;
    }

    return false;
}

namespace {

int pem_password_cb(char* buf, int size, int, void* userdata) {
    const auto* pass = static_cast<const std::string*>(userdata);
    if (!pass || pass->empty()) return 0;
    const int n = static_cast<int>(std::min<std::size_t>(pass->size(), static_cast<std::size_t>(size)));
    std::memcpy(buf, pass->data(), static_cast<std::size_t>(n));
    return n;
}

} // namespace

bool OpenSSLPrivateKey::load_pem(const std::string& pem, const std::string& passphrase) noexcept {
    release();
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (!bio) { m_last_error = "BIO allocation failed"; return false; }
    m_key = PEM_read_bio_PrivateKey(bio, nullptr, &pem_password_cb, const_cast<std::string*>(&passphrase));
    BIO_free(bio);

    if (!m_key) {
        m_last_error = "Could not parse PEM private key: " + openssl_last_error();
        return false;
    }

    return true;
}

bool OpenSSLPrivateKey::load_der(const std::uint8_t* der, std::size_t len, const std::string& passphrase) noexcept {
    release();
    if (!der || len == 0) { m_last_error = "Empty key"; return false; }
    const unsigned char* p = der;
    m_key = d2i_AutoPrivateKey(nullptr, &p, static_cast<long>(len));
    if (m_key) return true;
    openssl_drain_errors();
    p = der;
    X509_SIG* sig = d2i_X509_SIG(nullptr, &p, static_cast<long>(len));

    if (sig) {
        PKCS8_PRIV_KEY_INFO* info = PKCS8_decrypt(sig, passphrase.c_str(), static_cast<int>(passphrase.size()));
        X509_SIG_free(sig);

        if (info) {
            m_key = EVP_PKCS82PKEY(info);
            PKCS8_PRIV_KEY_INFO_free(info);
        }
    }

    if (!m_key) {
        m_last_error = "Could not parse DER private key: " + openssl_last_error();
        return false;
    }

    return true;
}

KeyType OpenSSLPrivateKey::key_type() const noexcept {
    if (!m_key) return KeyType::None;

    switch (EVP_PKEY_base_id(m_key)) {
        case EVP_PKEY_RSA:
        case EVP_PKEY_RSA_PSS: return KeyType::RSA;
        case EVP_PKEY_EC:      return KeyType::EC;
    #ifdef EVP_PKEY_ED25519
        case EVP_PKEY_ED25519: return KeyType::Ed25519;
    #endif
    #ifdef EVP_PKEY_ED448
        case EVP_PKEY_ED448:   return KeyType::Ed448;
    #endif
        case EVP_PKEY_DSA:     return KeyType::DSA;
        default:               return KeyType::Other;
    }
}

int OpenSSLPrivateKey::key_bits() const noexcept {
    return m_key ? EVP_PKEY_bits(m_key) : 0;
}

bool OpenSSLPrivateKey::matches_certificate(const NativeCertificateBase& cert) const noexcept {
    auto x = static_cast<X509*>(cert.native());
    if (!m_key || !x) return false;
    const bool ok = X509_check_private_key(x, m_key) == 1;
    openssl_drain_errors();
    return ok;
}

bool OpenSSLPrivateKey::to_pkcs8_der(std::vector<std::uint8_t>& out) const noexcept {
    if (!m_key) return false;
    PKCS8_PRIV_KEY_INFO* info = EVP_PKEY2PKCS8(m_key);
    if (!info) { openssl_drain_errors(); return false; }
    unsigned char* der = nullptr;
    const int len = i2d_PKCS8_PRIV_KEY_INFO(info, &der);
    PKCS8_PRIV_KEY_INFO_free(info);
    if (len <= 0 || !der) { openssl_drain_errors(); return false; }
    out.assign(der, der + len);
    OPENSSL_cleanse(der, static_cast<std::size_t>(len));
    OPENSSL_free(der);
    return true;
}

bool OpenSSLPrivateKey::build_pkcs12(const NativeCertificateBase& leaf, const std::vector<const NativeCertificateBase*>& chain,
                                     const std::string& password, std::vector<std::uint8_t>& out) const noexcept {
    auto leaf_x = static_cast<X509*>(leaf.native());
    if (!m_key || !leaf_x) return false;
    STACK_OF(X509)* ca = sk_X509_new_null();

    for (const auto* c : chain) {
        auto x = c ? static_cast<X509*>(c->native()) : nullptr;
        if (x && ca) sk_X509_push(ca, x);
    }

    PKCS12* p12 = PKCS12_create(password.c_str(), "fizmo", m_key, leaf_x, ca, 0, 0, 0, 0, 0);
    if (ca) sk_X509_free(ca);
    if (!p12) { openssl_drain_errors(); return false; }
    unsigned char* der = nullptr;
    const int len = i2d_PKCS12(p12, &der);
    PKCS12_free(p12);
    if (len <= 0 || !der) { openssl_drain_errors(); return false; }
    out.assign(der, der + len);
    OPENSSL_free(der);
    return true;
}

void OpenSSLSession::record_alert(bool received, int value) noexcept {
    m_alert.present = true;
    m_alert.received = received;
    m_alert.level = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    m_alert.description = static_cast<std::uint8_t>(value & 0xFF);
}

void OpenSSLSession::deliver_session(SSL_SESSION* session) noexcept {
    if (!m_session_cb || !session) return;
    unsigned char* der = nullptr;
    const int len = i2d_SSL_SESSION(session, &der);
    if (len <= 0 || !der) { openssl_drain_errors(); return; }
    std::vector<std::uint8_t> blob(der, der + len);
    OPENSSL_free(der);
    try { m_session_cb(std::move(blob)); } catch (...) {}
}

std::string OpenSSLSession::alpn_selected() const noexcept {
    if (!m_ssl) return {};
    const unsigned char* p = nullptr;
    unsigned int len = 0;
    SSL_get0_alpn_selected(m_ssl, &p, &len);
    return (p && len) ? std::string(reinterpret_cast<const char*>(p), len) : std::string();
}

std::string OpenSSLSession::requested_server_name() const noexcept {
    if (!m_ssl) return {};
    const char* n = SSL_get_servername(m_ssl, TLSEXT_NAMETYPE_host_name);
    return n ? std::string(n) : std::string();
}

bool OpenSSLSession::session_reused() const noexcept {
    return m_ssl && SSL_session_reused(m_ssl) == 1;
}

std::vector<std::uint8_t> OpenSSLSession::export_session() const noexcept {
    std::vector<std::uint8_t> out;
    if (!m_ssl || !m_done) return out;
    SSL_SESSION* s = SSL_get1_session(m_ssl);
    if (!s) return out;

    if (SSL_SESSION_is_resumable(s)) {
        unsigned char* der = nullptr;
        const int len = i2d_SSL_SESSION(s, &der);
        if (len > 0 && der) out.assign(der, der + len);
        if (der) OPENSSL_free(der);
    }

    SSL_SESSION_free(s);
    openssl_drain_errors();
    return out;
}

bool OpenSSLSession::import_session(const std::uint8_t* data, std::size_t len) noexcept {
    if (!m_ssl || m_server || !data || len == 0) return false;
    const unsigned char* p = data;
    SSL_SESSION* s = d2i_SSL_SESSION(nullptr, &p, static_cast<long>(len));
    if (!s) { openssl_drain_errors(); return false; }
    const bool ok = SSL_set_session(m_ssl, s) == 1;
    SSL_SESSION_free(s);
    openssl_drain_errors();
    return ok;
}

bool OpenSSLSession::export_keying_material(const std::string& label, const std::uint8_t* context, std::size_t context_len,
                                            bool use_context, std::size_t out_len, std::vector<std::uint8_t>& out) const noexcept {
    if (!m_ssl || !m_done || out_len == 0) return false;
    out.assign(out_len, 0);
    static const unsigned char empty = 0;

    const int ok = SSL_export_keying_material(m_ssl, out.data(), out_len, label.c_str(), label.size(),
                                              context ? context : &empty, context_len, use_context ? 1 : 0);

    if (ok != 1) { out.clear(); openssl_drain_errors(); return false; }
    return true;
}

bool OpenSSLSession::channel_binding(ChannelBindingType type, std::vector<std::uint8_t>& out) const noexcept {
    if (!m_ssl || !m_done) return false;
    out.clear();

    switch (type) {
        case ChannelBindingType::TlsExporter: {
            if (SSL_version(m_ssl) < TLS1_3_VERSION) return false;
            return export_keying_material("EXPORTER-Channel-Binding", nullptr, 0, true, 32, out);
        }

        case ChannelBindingType::TlsUnique: {
            if (SSL_version(m_ssl) >= TLS1_3_VERSION) return false;
            unsigned char buf[64];
            const bool reused = SSL_session_reused(m_ssl) == 1;
            const bool own = (!m_server && !reused) || (m_server && reused);
            const std::size_t n = own ? SSL_get_finished(m_ssl, buf, sizeof(buf)) : SSL_get_peer_finished(m_ssl, buf, sizeof(buf));
            if (n == 0 || n > sizeof(buf)) return false;
            out.assign(buf, buf + n);
            return true;
        }

        case ChannelBindingType::TlsServerEndPoint: {
            X509* cert = nullptr;
            bool owned = false;

            if (m_server) {
                cert = SSL_get_certificate(m_ssl);
            } else {
                cert = FIZMO_SSL_GET1_PEER_CERT(m_ssl);
                owned = true;
            }

            if (!cert) return false;
            int md_nid = NID_undef, pk_nid = NID_undef;
            OBJ_find_sigid_algs(X509_get_signature_nid(cert), &md_nid, &pk_nid);
            const EVP_MD* md = nullptr;
            if (md_nid == NID_md5 || md_nid == NID_sha1 || md_nid == NID_undef) md = EVP_sha256();
            else md = EVP_get_digestbynid(md_nid);
            if (!md) md = EVP_sha256();
            unsigned char digest[EVP_MAX_MD_SIZE];
            unsigned int dlen = 0;
            const bool ok = X509_digest(cert, md, digest, &dlen) == 1;
            if (owned) X509_free(cert);
            if (!ok) { openssl_drain_errors(); return false; }
            out.assign(digest, digest + dlen);
            return true;
        }
    }

    return false;
}

std::vector<std::uint8_t> OpenSSLSession::ocsp_response() const noexcept {
    std::vector<std::uint8_t> out;
    if (!m_ssl) return out;
    const unsigned char* p = nullptr;
    const long len = SSL_get_tlsext_status_ocsp_resp(m_ssl, &p);
    if (len > 0 && p) out.assign(p, p + len);
    return out;
}

bool OpenSSLSession::local_certificate(std::vector<std::uint8_t>& out) const noexcept {
    if (!m_ssl) return false;
    X509* cert = SSL_get_certificate(m_ssl);
    if (!cert) return false;
    std::vector<std::vector<std::uint8_t>> tmp;
    if (!append_der(cert, tmp)) return false;
    out = std::move(tmp.front());
    return true;
}

bool OpenSSLSession::key_update(bool request_peer, std::vector<std::uint8_t>& out) noexcept {
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    if (!m_ssl || !m_done || SSL_version(m_ssl) < TLS1_3_VERSION) return false;
    if (SSL_key_update(m_ssl, request_peer ? SSL_KEY_UPDATE_REQUESTED : SSL_KEY_UPDATE_NOT_REQUESTED) != 1) { openssl_drain_errors(); return false; }
    ERR_clear_error();
    SSL_do_handshake(m_ssl);
    openssl_drain_errors();
    drain_wbio(out);
    return !out.empty();
#else
    (void)request_peer;
    (void)out;
    return false;
#endif
}

bool OpenSSLSession::request_post_handshake_auth(std::vector<std::uint8_t>& out) noexcept {
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    if (!m_ssl || !m_done || !m_server || SSL_version(m_ssl) < TLS1_3_VERSION) return false;
    SSL_set_verify(m_ssl, SSL_VERIFY_PEER | SSL_VERIFY_POST_HANDSHAKE, nullptr);
    if (SSL_verify_client_post_handshake(m_ssl) != 1) { openssl_drain_errors(); return false; }
    ERR_clear_error();
    SSL_do_handshake(m_ssl);
    openssl_drain_errors();
    drain_wbio(out);
    return !out.empty();
#else
    (void)out;
    return false;
#endif
}
#endif

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo
