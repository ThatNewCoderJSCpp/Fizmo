#include "fizmo_library.hpp"
#include "tls.hpp"
#include "../Security/certificate_chain.hpp"
#include "../Security/private_key.hpp"
#include "../Security/cipher_suites.hpp"

namespace fizmo {
namespace networking {
namespace tls {

namespace {

std::vector<std::string> split_ciphers(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;

    for (char c : s) {
        if (c == ':' || c == ',' || c == ' ') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }

    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool fail(std::string* error, const std::string& why) {
    if (error) *error = why;
    return false;
}

bool configure(security::TLSContext& ctx, const SecureContextOptions& o, bool server, std::string* error) {
    ctx.set_protocol_range(o.min_version, o.max_version);
    ctx.set_check_revocation(o.check_revocation);
    ctx.set_session_tickets(o.session_tickets);
    if (!o.alpn_protocols.empty()) ctx.set_alpn_protocols(o.alpn_protocols);
    if (!o.ca_file.empty()) ctx.set_ca_store_path(o.ca_file);
    if (!o.crl_files.empty()) ctx.set_crl_files(o.crl_files);

    if (!o.ciphers.empty()) {
        std::vector<std::string> unknown;
        auto list = security::CipherList::from_names(split_ciphers(o.ciphers), &unknown);
        if (!unknown.empty()) return fail(error, "Unknown cipher suite: " + unknown.front());
        if (list.empty()) return fail(error, "No usable cipher suites in \"" + o.ciphers + "\"");
        ctx.set_cipher_list(list);
    }

    if (server) {
        if (o.request_cert) ctx.set_verify_mode(o.reject_unauthorized ? security::VerifyMode::FailIfNoPeer : security::VerifyMode::Peer);
        else ctx.set_verify_mode(security::VerifyMode::None);
    } else {
        ctx.set_verify_mode(o.reject_unauthorized ? security::VerifyMode::Peer : security::VerifyMode::None);
    }

    if (!o.pfx_file.empty()) {
        if (!ctx.set_pkcs12_file(o.pfx_file, o.passphrase)) return fail(error, "Could not load PKCS#12 file \"" + o.pfx_file + "\": " + ctx.last_error());
    } else if (!o.cert_file.empty() || !o.key_file.empty()) {
        if (o.cert_file.empty() || o.key_file.empty()) return fail(error, "Both cert_file and key_file are required");
        if (!ctx.set_certificate_files(o.cert_file, o.key_file, o.passphrase)) return fail(error, "Could not load certificate files: " + ctx.last_error());
    } else if (!o.cert.empty() || !o.key.empty()) {
        if (o.cert.empty() || o.key.empty()) return fail(error, "Both cert and key are required");
        auto chain = security::CertificateChain::from_pem_string(o.cert);
        if (chain.length() == 0) return fail(error, "Could not parse PEM certificate");
        security::PrivateKey key;
        if (!key.load_from_pem(o.key, o.passphrase)) return fail(error, "Could not parse PEM private key");
        if (!ctx.set_certificate_chain(chain, key)) return fail(error, "Could not use certificate: " + ctx.last_error());
    } else if (server) {
        return fail(error, "A TLS server needs a certificate (cert/key, cert_file/key_file or pfx_file)");
    }

    if (!ctx.initialize()) return fail(error, "TLS initialisation failed: " + ctx.last_error());
    return true;
}

} // namespace

std::shared_ptr<security::TLSContext> create_secure_context(const SecureContextOptions& options, bool server, std::string* error) {
    auto ctx = std::make_shared<security::TLSContext>(server ? security::ContextUse::Server : security::ContextUse::Client);
    if (!configure(*ctx, options, server, error)) return nullptr;
    return ctx;
}

net::Server create_server(const ServerOptions& options, std::function<void(net::Socket)> on_secure_connection) {
    std::shared_ptr<security::TLSContext> ctx = options.secure_context;
    std::string why;
    if (!ctx) ctx = create_secure_context(options.secure, true, &why);

    if (!ctx) {
        ctx = std::make_shared<security::TLSContext>(security::ContextUse::Server);
        net::Server s = net::detail::make_server(options, ctx);
        if (on_secure_connection) s.on_connection(std::move(on_secure_connection));
        loop().post([s, why]() mutable {
            const auto ec = async::make_error_code(async::error::tls_error);
            if (s.events().error.emit(ec) == 0) report_unhandled_error("tls::create_server: " + why, ec);
        });
        return s;
    }

    net::Server s = net::detail::make_server(options, ctx);
    if (on_secure_connection) s.on_connection(std::move(on_secure_connection));
    return s;
}

net::Server create_server(const SecureContextOptions& options, std::function<void(net::Socket)> on_secure_connection) {
    ServerOptions o;
    o.secure = options;
    return create_server(o, std::move(on_secure_connection));
}

net::Socket connect(const ConnectOptions& options, std::function<void()> on_secure_connect) {
    std::shared_ptr<security::TLSContext> ctx = options.secure_context;
    std::string why;
    if (!ctx) ctx = create_secure_context(options.secure, false, &why);

    if (!ctx) {
        net::Socket s = net::detail::make_failed(async::make_error_code(async::error::tls_error), why, true);
        if (on_secure_connect) s.events().secure_connect.once(std::move(on_secure_connect));
        return s;
    }

    net::Socket s = net::detail::make_client(options, ctx, options.servername);
    if (on_secure_connect) s.events().secure_connect.once(std::move(on_secure_connect));
    return s;
}

net::Socket connect(std::uint16_t port, const std::string& host, std::function<void()> on_secure_connect, const SecureContextOptions& options) {
    ConnectOptions o;
    o.port = port;
    o.host = host.empty() ? "localhost" : host;
    o.secure = options;
    return connect(o, std::move(on_secure_connect));
}

std::vector<std::string> get_ciphers() {
    std::vector<std::string> out;
    for (auto& c : security::all_cipher_suites()) out.push_back(c.openssl_name ? c.openssl_name : c.iana_name);
    return out;
}

const char* backend_name() noexcept { return security::TLSContext::backend_name(); }

} // namespace tls
} // namespace networking
} // namespace fizmo
