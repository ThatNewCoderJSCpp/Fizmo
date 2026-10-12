#ifndef FIZMO_NODE_TLS_HPP
#define FIZMO_NODE_TLS_HPP

#include "net.hpp"

#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace tls {

struct SecureContextOptions {
    std::string                cert;
    std::string                key;
    std::string                passphrase;
    std::string                cert_file;
    std::string                key_file;
    std::string                pfx_file;
    std::string                ca_file;
    std::vector<std::string>   crl_files;
    std::vector<std::string>   alpn_protocols;
    std::string                ciphers;
    security::TLSVersion       min_version = security::TLSVersion::TLS_1_2;
    security::TLSVersion       max_version = security::TLSVersion::TLS_1_3;
    bool                       reject_unauthorized = true;
    bool                       request_cert = false;
    bool                       check_revocation = false;
    bool                       session_tickets = true;
};

struct ConnectOptions : net::ConnectOptions {
    SecureContextOptions                  secure;
    std::string                           servername;
    std::shared_ptr<security::TLSContext> secure_context;
};

struct ServerOptions : net::ServerOptions {
    SecureContextOptions                  secure;
    std::shared_ptr<security::TLSContext> secure_context;
};

std::shared_ptr<security::TLSContext> create_secure_context(const SecureContextOptions& options, bool server, std::string* error = nullptr);

net::Server create_server(const ServerOptions& options, std::function<void(net::Socket)> on_secure_connection = {});
net::Server create_server(const SecureContextOptions& options, std::function<void(net::Socket)> on_secure_connection = {});

net::Socket connect(const ConnectOptions& options, std::function<void()> on_secure_connect = {});
net::Socket connect(std::uint16_t port, const std::string& host = "localhost", std::function<void()> on_secure_connect = {}, const SecureContextOptions& options = SecureContextOptions());

std::vector<std::string> get_ciphers();
const char* backend_name() noexcept;

} // namespace tls
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_TLS_HPP
