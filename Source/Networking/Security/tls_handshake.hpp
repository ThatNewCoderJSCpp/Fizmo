#ifndef FIZMO_TLS_HANDSHAKE_HPP
#define FIZMO_TLS_HANDSHAKE_HPP

#include "tls_context.hpp"
#include "tls_record.hpp"
#include "certificate_verifier.hpp"
#include "certificate_chain.hpp"
#include "Backend/native_backend.hpp"
#include "../Core/socket_error.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>

namespace fizmo {
namespace networking {
namespace security {

enum class HandshakeRole : std::uint8_t {
    Client = 0,
    Server
};

enum class HandshakeState : std::uint8_t {
    NotStarted = 0,
    InProgress,             // more round trips to go
    NeedMoreData,           // waiting for peer bytes
    PeerCertificateCheck,   // paused for certificate verification
    Renegotiating,          // mid-connection renegotiation / key update
    Complete,               // keys are ready
    Failed                  // terminal error
};

const char* handshake_state_to_string(HandshakeState s) noexcept;

enum class HandshakeError : std::uint8_t {
    None = 0,
    InternalError,
    ProtocolError,
    CertificateRequired,
    CertificateInvalid,
    HostnameMismatch,
    AlgorithmMismatch,
    Timeout,
    PeerRejected,
    ContextNotInitialised
};

const char* handshake_error_to_string(HandshakeError e) noexcept;

struct HandshakeResult {
    HandshakeState            state = HandshakeState::NotStarted;
    HandshakeError            error = HandshakeError::None;
    std::string               detail;
    std::vector<std::uint8_t> output;   
    std::vector<std::uint8_t> extra;   

    bool is_complete() const noexcept { return state == HandshakeState::Complete; }
    bool has_failed()  const noexcept { return state == HandshakeState::Failed; }
    bool needs_input() const noexcept { return state == HandshakeState::NeedMoreData; }
    bool has_output()  const noexcept { return !output.empty(); }

    std::string to_string() const;
};

class TLSHandshake {
public:
    TLSHandshake(TLSContext& ctx, HandshakeRole role, const std::string& hostname = {}) noexcept : m_ctx(ctx), m_role(role), m_hostname(hostname), m_state(HandshakeState::NotStarted) {}

    ~TLSHandshake() noexcept { m_session.close_session(); }

    TLSHandshake(const TLSHandshake&)            = delete;
    TLSHandshake& operator=(const TLSHandshake&) = delete;
    TLSHandshake(TLSHandshake&&)                 = delete;
    TLSHandshake& operator=(TLSHandshake&&)      = delete;

public:
    void set_on_complete(std::function<void()> cb) { m_on_complete = std::move(cb); }
    void set_on_failed(std::function<void(HandshakeError, const std::string&)> cb) { m_on_failed = std::move(cb); }
    void set_verifier(std::shared_ptr<CertificateVerifier> v) { m_verifier = std::move(v); }

    HandshakeState     state()    const noexcept { return m_state; }
    HandshakeRole      role()     const noexcept { return m_role; }
    bool               is_complete() const noexcept { return m_state == HandshakeState::Complete; }
    bool               has_failed()  const noexcept { return m_state == HandshakeState::Failed; }
    const std::string& hostname() const noexcept { return m_hostname; }

    detail::NativeSession&       session()       noexcept { return m_session; }
    const detail::NativeSession& session() const noexcept { return m_session; }

public:
    HandshakeResult start() noexcept;

    HandshakeResult step(const std::uint8_t* data, std::size_t len) noexcept;

    HandshakeResult step(const std::vector<std::uint8_t>& data) noexcept {
        return step(data.data(), data.size());
    }

    HandshakeResult renegotiate(const std::uint8_t* data, std::size_t len) noexcept {
        m_state = HandshakeState::Renegotiating;
        return drive(data, len);
    }

public:
    std::string to_string() const;

private:
    TLSContext&                          m_ctx;
    HandshakeRole                        m_role;
    std::string                          m_hostname;
    HandshakeState                       m_state;
    detail::NativeSession                m_session;

    std::function<void()>                                  m_on_complete;
    std::function<void(HandshakeError, const std::string&)> m_on_failed;
    std::shared_ptr<CertificateVerifier>                   m_verifier;

private:
    HandshakeResult drive(const std::uint8_t* data, std::size_t len) noexcept;

    static HandshakeError classify(const detail::HandshakeIO& io) noexcept;

    HandshakeResult verify_peer() noexcept;

    std::string verification_hostname() const {
        return (m_role == HandshakeRole::Server) ? std::string() : m_hostname;
    }

    HandshakeResult apply(const VerifyResult& vr) noexcept;

    HandshakeResult fail(HandshakeError err, const std::string& msg = {}) noexcept;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_HANDSHAKE_HPP