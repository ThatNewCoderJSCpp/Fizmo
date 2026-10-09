#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "tls_handshake.hpp"

namespace fizmo {
namespace networking {
namespace security {

const char* handshake_state_to_string(HandshakeState s) noexcept {
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

const char* handshake_error_to_string(HandshakeError e) noexcept {
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

auto HandshakeResult::to_string() const -> std::string {
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

auto TLSHandshake::start() noexcept -> HandshakeResult {
        if (!m_ctx.is_initialized()) {
            return fail(HandshakeError::ContextNotInitialised, "TLSContext not initialised");
        }

        if (!m_session.open(m_ctx.credentials(), m_role == HandshakeRole::Server, m_hostname)) {
            return fail(HandshakeError::InternalError, "Could not open a TLS session");
        }

        m_state = HandshakeState::InProgress;
        return drive(nullptr, 0);
    }

auto TLSHandshake::step(const std::uint8_t* data, std::size_t len) noexcept -> HandshakeResult {
        if (m_state == HandshakeState::Complete || m_state == HandshakeState::Failed) {
            return { m_state, HandshakeError::None, {}, {}, {} };
        }

        return drive(data, len);
    }

auto TLSHandshake::to_string() const -> std::string {
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

auto TLSHandshake::drive(const std::uint8_t* data, std::size_t len) noexcept -> HandshakeResult {
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

auto TLSHandshake::classify(const detail::HandshakeIO& io) noexcept -> HandshakeError {
        if (io.detail.find("name mismatch") != std::string::npos) return HandshakeError::HostnameMismatch;
        if (io.detail.find("cipher") != std::string::npos)        return HandshakeError::AlgorithmMismatch;
        if (io.detail.find("Certificate") != std::string::npos)   return HandshakeError::CertificateInvalid;
        if (io.detail.find("Credentials") != std::string::npos)   return HandshakeError::CertificateRequired;
        return HandshakeError::InternalError;
    }

auto TLSHandshake::verify_peer() noexcept -> HandshakeResult {
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

auto TLSHandshake::apply(const VerifyResult& vr) noexcept -> HandshakeResult {
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

auto TLSHandshake::fail(HandshakeError err, const std::string& msg) noexcept -> HandshakeResult {
        m_state = HandshakeState::Failed;

        if (m_on_failed) {
            try { m_on_failed(err, msg); } catch (...) {}
        }

        return { HandshakeState::Failed, err, msg, {}, {} };
    }

} // namespace security
} // namespace networking
} // namespace fizmo
