#ifndef FIZMO_CLIENT_HELLO_HPP
#define FIZMO_CLIENT_HELLO_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace security {

enum class ClientHelloStatus : std::uint8_t {
    Complete,
    NeedMoreData,
    NotTls,
    Malformed
};

struct ClientHelloInfo {
    std::uint16_t              legacy_version = 0;
    std::string                server_name;
    std::vector<std::string>   alpn_protocols;
    std::vector<std::uint16_t> supported_versions;
    std::vector<std::uint16_t> cipher_suites;
    std::vector<std::uint16_t> supported_groups;
    std::vector<std::uint16_t> signature_algorithms;
    std::vector<std::uint16_t> extensions;
    std::vector<std::uint8_t>  session_id;
    bool                       offers_tls13 = false;
    bool                       offers_early_data = false;
    bool                       offers_post_handshake_auth = false;
    bool                       requests_ocsp = false;
};

ClientHelloStatus parse_client_hello(const std::uint8_t* data, std::size_t len, ClientHelloInfo& out);

inline ClientHelloStatus parse_client_hello(const std::vector<std::uint8_t>& data, ClientHelloInfo& out) {
    return parse_client_hello(data.data(), data.size(), out);
}

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_CLIENT_HELLO_HPP
