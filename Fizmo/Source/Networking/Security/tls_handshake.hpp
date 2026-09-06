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

inline const char* handshake_state_to_string(HandshakeState s) noexcept {
    switch (s) {
        case HandshakeState::NotStarted:           return "NotStarted";
        case HandshakeState::InProgress:           return "InProgress";
        case HandshakeState::NeedMoreData:         return "NeedMoreData";
        case HandshakeState::PeerCertificateCheck: return "PeerCertificateCheck";
        case HandshakeState::Renegotiating:        return "Renegotiating";
        case HandshakeState::Complete:             return "Complete";
        case HandshakeState::Failed:               return "Failed";
        default:                                   return "Unknown";
    }
}

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

inline const char* handshake_error_to_string(HandshakeError e) noexcept {
    switch (e) {
        case HandshakeError::None:                  return "None";
        case HandshakeError::InternalError:         return "InternalError";
        case HandshakeError::ProtocolError:         return "ProtocolError";
        case HandshakeError::CertificateRequired:   return "CertificateRequired";
        case HandshakeError::CertificateInvalid:    return "CertificateInvalid";
        case HandshakeError::HostnameMismatch:      return "HostnameMismatch";
        case HandshakeError::AlgorithmMismatch:     return "AlgorithmMismatch";
        case HandshakeError::Timeout:               return "Timeout";
        case HandshakeError::PeerRejected:          return "PeerRejected";
        case HandshakeError::ContextNotInitialised: return "ContextNotInitialised";
        default:                                    return "Unknown";
    }
}

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

    std::string to_string() const {
        std::string s = "HandshakeResult[";
        s += handshake_state_to_string(state);

        if (error != HandshakeError::None) {
            s += " error=";
            s += handshake_error_to_string(error);
        }

        if (!detail.empty()) {
            s += " (";
            s += detail;
            s += ')';
        }

        if (!output.empty()) {
            s += " out=";
            s += std::to_string(output.size());
            s += 'B';
        }

        s += ']';
        return s;
    }
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
    HandshakeResult start() noexcept {
        if (!m_ctx.is_initialized()) {
            return fail(HandshakeError::ContextNotInitialised, "TLSContext not initialised");
        }

        if (!m_session.open(m_ctx.credentials(), m_role == HandshakeRole::Server, m_hostname)) {
            return fail(HandshakeError::InternalError, "Could not open a TLS session");
        }

        m_state = HandshakeState::InProgress;
        return drive(nullptr, 0);
    }

    HandshakeResult step(const std::uint8_t* data, std::size_t len) noexcept {
        if (m_state == HandshakeState::Complete || m_state == HandshakeState::Failed) {
            return { m_state, HandshakeError::None, {}, {}, {} };
        }

        return drive(data, len);
    }

    HandshakeResult step(const std::vector<std::uint8_t>& data) noexcept {
        return step(data.data(), data.size());
    }

    HandshakeResult renegotiate(const std::uint8_t* data, std::size_t len) noexcept {
        m_state = HandshakeState::Renegotiating;
        return drive(data, len);
    }

public:
    std::string to_string() const {
        std::string s = "TLSHandshake[";
        s += (m_role == HandshakeRole::Client) ? "Client" : "Server";
        s += " state=";
        s += handshake_state_to_string(m_state);

        if (!m_hostname.empty()) {
            s += " host=\"";
            s += m_hostname;
            s += '"';
        }

        s += ']';
        return s;
    }

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
    HandshakeResult drive(const std::uint8_t* data, std::size_t len) noexcept {
        detail::HandshakeIO io = m_session.handshake(data, len);
        HandshakeResult result;
        result.output = std::move(io.outgoing);
        result.extra  = std::move(io.leftover);

        switch (io.status) {
            case detail::HandshakeStatus::Continue:
                m_state = HandshakeState::InProgress;
                result.state = HandshakeState::InProgress;
                return result;

            case detail::HandshakeStatus::NeedMoreData:
                m_state = HandshakeState::NeedMoreData;
                result.state = HandshakeState::NeedMoreData;
                return result;

            case detail::HandshakeStatus::Complete: {
                HandshakeResult verification = verify_peer();
                if (verification.has_failed()) { return verification; }
                m_state = HandshakeState::Complete;
                result.state = HandshakeState::Complete;
                if (m_on_complete) { try { m_on_complete(); } catch (...) {} }
                return result;
            }

            case detail::HandshakeStatus::Failed:
            default:
                return fail(classify(io), io.detail.empty() ? "Handshake failed" : io.detail);
        }
    }

    static HandshakeError classify(const detail::HandshakeIO& io) noexcept {
        if (io.detail.find("name mismatch") != std::string::npos) return HandshakeError::HostnameMismatch;
        if (io.detail.find("cipher") != std::string::npos)        return HandshakeError::AlgorithmMismatch;
        if (io.detail.find("Certificate") != std::string::npos)   return HandshakeError::CertificateInvalid;
        if (io.detail.find("Credentials") != std::string::npos)   return HandshakeError::CertificateRequired;
        return HandshakeError::InternalError;
    }

    HandshakeResult verify_peer() noexcept {
        const VerifyMode mode = m_ctx.verify_mode();

        if (mode == VerifyMode::None) {
            return { HandshakeState::Complete, HandshakeError::None, {}, {}, {} };
        }

        std::vector<std::vector<std::uint8_t>> ders;

        if (!m_session.peer_chain(ders) || ders.empty()) {
            if (mode == VerifyMode::FailIfNoPeer) {
                return fail(HandshakeError::CertificateRequired, "Peer did not present a certificate (FailIfNoPeer)");
            }

            if (m_role == HandshakeRole::Client) {
                return fail(HandshakeError::CertificateRequired, "Server did not present a certificate");
            }

            return { HandshakeState::Complete, HandshakeError::None, {}, {}, {} };
        }

        CertificateChain chain = CertificateChain::from_der_list(ders);

        if (chain.empty()) {
            return fail(HandshakeError::CertificateInvalid, "Peer certificate could not be parsed");
        }

        if (!m_verifier) {
            CertificateVerifier fallback;
            fallback.set_ca_store_path(m_ctx.ca_store_path());
            fallback.set_check_revocation(m_ctx.check_revocation());
            return apply(fallback.verify(verification_hostname(), chain.leaf(), &chain));
        }

        return apply(m_verifier->verify(verification_hostname(), chain.leaf(), &chain));
    }

    std::string verification_hostname() const {
        return (m_role == HandshakeRole::Server) ? std::string() : m_hostname;
    }

    HandshakeResult apply(const VerifyResult& vr) noexcept {
        if (vr.passed) { return { HandshakeState::Complete, HandshakeError::None, {}, {}, {} }; }

        HandshakeError err = HandshakeError::CertificateInvalid;
        if (vr.error == VerifyError::HostnameMismatch) { err = HandshakeError::HostnameMismatch; }

        return fail(
            err,
            std::string("Certificate verification failed: ") +
            verify_error_to_string(vr.error) +
            (vr.detail.empty() ? "" : " - " + vr.detail)
        );
    }

    HandshakeResult fail(HandshakeError err, const std::string& msg = {}) noexcept {
        m_state = HandshakeState::Failed;

        if (m_on_failed) {
            try { m_on_failed(err, msg); } catch (...) {}
        }

        return { HandshakeState::Failed, err, msg, {}, {} };
    }
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_HANDSHAKE_HPP