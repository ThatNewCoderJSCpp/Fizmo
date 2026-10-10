#ifndef FIZMO_TLS_BACKEND_BASE_HPP
#define FIZMO_TLS_BACKEND_BASE_HPP

#include "tls_types.hpp"
#include <string>
#include <vector>
#include <cstdint>

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
};

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_BACKEND_BASE_HPP