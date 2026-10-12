#ifndef FIZMO_TLS_BACKEND_BASE_HPP
#define FIZMO_TLS_BACKEND_BASE_HPP

#include "tls_types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace fizmo {
namespace networking {
namespace security {
namespace detail {

enum class HandshakeStatus : std::uint8_t {
    Continue = 0,      
    NeedMoreData,  
    Complete,      
    Failed
};

enum class IOStatus : std::uint8_t {
    Ok = 0,
    NeedMoreData,   
    Renegotiate,    
    Closed,         
    Failed
};

struct HandshakeIO {
    HandshakeStatus           status = HandshakeStatus::Failed;
    std::vector<std::uint8_t> outgoing;   
    std::vector<std::uint8_t> leftover;   
    int                       native_code = 0;
    std::string               detail;
};

struct DecryptIO {
    IOStatus                  status = IOStatus::Failed;
    std::vector<std::uint8_t> plaintext;
    std::vector<std::uint8_t> outgoing;   
    std::vector<std::uint8_t> leftover;  
    int                       native_code = 0;
};

class NativeCertificateBase {
public:
    virtual ~NativeCertificateBase() = default;

    virtual bool parse(const std::uint8_t* der, std::size_t len) noexcept = 0;
    virtual void release() noexcept = 0;
    virtual bool valid() const noexcept = 0;
    virtual const CertificateInfo& info() const noexcept = 0;
    virtual bool self_signed() const noexcept = 0;
    virtual void* native() const noexcept = 0;
};

class NativePrivateKeyBase {
public:
    virtual ~NativePrivateKeyBase() = default;

    virtual bool load_pem(const std::string& pem, const std::string& passphrase) noexcept = 0;
    virtual bool load_der(const std::uint8_t* der, std::size_t len, const std::string& passphrase) noexcept = 0;
    virtual void release() noexcept = 0;
    virtual bool valid() const noexcept = 0;
    virtual KeyType key_type() const noexcept = 0;
    virtual int key_bits() const noexcept = 0;
    virtual bool matches_certificate(const NativeCertificateBase& cert) const noexcept = 0;
    virtual bool to_pkcs8_der(std::vector<std::uint8_t>& out) const noexcept = 0;
    virtual bool build_pkcs12(const NativeCertificateBase& leaf, const std::vector<const NativeCertificateBase*>& chain,
                              const std::string& password, std::vector<std::uint8_t>& out) const noexcept = 0;
    virtual void* native() const noexcept = 0;
    virtual const std::string& last_error() const noexcept = 0;
};

class NativeCredentialsBase {
public:
    virtual ~NativeCredentialsBase() = default;

    virtual bool acquire(const TLSConfig& config) noexcept = 0;
    virtual void release() noexcept = 0;
    virtual bool acquired() const noexcept = 0;
    virtual const std::string& last_error() const noexcept = 0;
    virtual void* native() const noexcept = 0;
};

class NativeSessionBase {
public:
    virtual ~NativeSessionBase() = default;

    virtual bool open(NativeCredentialsBase& credentials, bool server, const std::string& hostname) noexcept = 0;
    virtual void close_session() noexcept = 0;
    virtual bool established() const noexcept = 0;

    virtual HandshakeIO handshake(const std::uint8_t* input, std::size_t length) noexcept = 0;
    virtual int         encrypt(const std::uint8_t* plaintext, std::size_t length, std::vector<std::uint8_t>& out) noexcept = 0;
    virtual DecryptIO   decrypt(const std::uint8_t* ciphertext, std::size_t length) noexcept = 0;
    virtual bool        close_notify(std::vector<std::uint8_t>& out) noexcept = 0;

    virtual bool peer_chain(std::vector<std::vector<std::uint8_t>>& out) const noexcept = 0;

    virtual std::string cipher_name() const noexcept = 0;
    virtual TLSVersion  protocol_version() const noexcept = 0;
    virtual std::size_t max_plaintext_chunk() const noexcept = 0;

    virtual std::string alpn_selected() const noexcept { return {}; }
    virtual std::string requested_server_name() const noexcept { return {}; }
    virtual bool        session_reused() const noexcept { return false; }

    virtual std::vector<std::uint8_t> export_session() const noexcept { return {}; }
    virtual bool import_session(const std::uint8_t*, std::size_t) noexcept { return false; }
    virtual void set_session_callback(std::function<void(std::vector<std::uint8_t>)>) noexcept {}

    virtual bool export_keying_material(const std::string&, const std::uint8_t*, std::size_t, bool, std::size_t, std::vector<std::uint8_t>&) const noexcept { return false; }
    virtual bool channel_binding(ChannelBindingType, std::vector<std::uint8_t>&) const noexcept { return false; }

    virtual AlertInfo last_alert() const noexcept { return {}; }
    virtual std::vector<std::uint8_t> ocsp_response() const noexcept { return {}; }
    virtual bool local_certificate(std::vector<std::uint8_t>&) const noexcept { return false; }

    virtual bool key_update(bool, std::vector<std::uint8_t>&) noexcept { return false; }
    virtual bool request_post_handshake_auth(std::vector<std::uint8_t>&) noexcept { return false; }
};

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_BACKEND_BASE_HPP