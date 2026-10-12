#include "fizmo_library.hpp"
#include "tls_handshake.hpp"
#include "certificate_pinning.hpp"
#include "ocsp.hpp"

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

std::string HandshakeResult::to_string() const {
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

TLSHandshake::TLSHandshake(TLSContext& ctx, HandshakeRole role, const std::string& hostname) noexcept
    : m_ctx(ctx), m_active(&ctx), m_role(role), m_hostname(hostname), m_state(HandshakeState::NotStarted), m_verify(VerifyResult::success()) {}

bool TLSHandshake::open_session(TLSContext& ctx) noexcept {
    m_active = &ctx;
    if (!m_session.open(ctx.credentials(), m_role == HandshakeRole::Server, m_hostname)) return false;

    if (m_role == HandshakeRole::Client && !m_session_key.empty()) {
        try {
            auto cache = ctx.session_cache();

            if (cache) {
                std::vector<std::uint8_t> saved;
                if (cache->lookup(m_session_key, saved)) m_session.import_session(saved.data(), saved.size());
                std::weak_ptr<TLSSessionCache> weak = cache;
                const std::string key = m_session_key;

                m_session.set_session_callback([weak, key](std::vector<std::uint8_t> blob) {
                    if (auto c = weak.lock()) c->store(key, std::move(blob));
                });
            }
        } catch (...) {}
    }

    return true;
}

auto TLSHandshake::start() noexcept -> HandshakeResult {
    if (!m_ctx.is_initialized()) {
        return fail(HandshakeError::ContextNotInitialised, "TLSContext not initialised");
    }

    m_metrics.start_handshake();
    try { m_ctx.metrics()->on_handshake_started(); } catch (...) {}
    m_state = HandshakeState::InProgress;

    if (m_role == HandshakeRole::Server) {
        m_deferred = true;
        m_state = HandshakeState::NeedMoreData;
        return { HandshakeState::NeedMoreData, HandshakeError::None, {}, {}, {} };
    }

    if (!open_session(m_ctx)) {
        return fail(HandshakeError::InternalError, "Could not open a TLS session");
    }

    return drive(nullptr, 0);
}

auto TLSHandshake::step(const std::uint8_t* data, std::size_t len) noexcept -> HandshakeResult {
    if (m_state == HandshakeState::Complete || m_state == HandshakeState::Failed) {
        return { m_state, HandshakeError::None, {}, {}, {} };
    }

    if (m_deferred) {
        try {
            if (data && len) m_hello_buf.insert(m_hello_buf.end(), data, data + len);
        } catch (...) {
            return fail(HandshakeError::InternalError, "Out of memory");
        }

        const ClientHelloStatus st = parse_client_hello(m_hello_buf, m_hello);

        if (st == ClientHelloStatus::NeedMoreData) {
            m_state = HandshakeState::NeedMoreData;
            return { HandshakeState::NeedMoreData, HandshakeError::None, {}, {}, {} };
        }

        m_deferred = false;
        TLSContext* chosen = &m_ctx;

        if (st == ClientHelloStatus::Complete) {
            m_sni = m_hello.server_name;

            try {
                auto cb = m_ctx.sni_callback();

                if (cb) {
                    TLSContext* alt = cb(m_sni);

                    if (alt && alt != &m_ctx) {
                        if (!alt->is_initialized() && !alt->initialize()) {
                            return fail(HandshakeError::ContextNotInitialised, "SNI context could not be initialised: " + alt->last_error());
                        }

                        chosen = alt;
                    }
                }
            } catch (...) {
                return fail(HandshakeError::InternalError, "SNI callback threw");
            }
        }

        if (!open_session(*chosen)) return fail(HandshakeError::InternalError, "Could not open a TLS session");
        std::vector<std::uint8_t> buffered;
        buffered.swap(m_hello_buf);
        return drive(buffered.data(), buffered.size());
    }

    return drive(data, len);
}

std::string TLSHandshake::to_string() const {
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
            if (verification.has_failed()) {
                verification.output = std::move(result.output);
                return verification;
            }
            m_state = HandshakeState::Complete;
            result.state = HandshakeState::Complete;
            m_metrics.finish_handshake(m_session.session_reused());

            try {
                m_metrics.protocol = tls_version_to_string(m_session.protocol_version());
                m_metrics.cipher = m_session.cipher_name();
                m_metrics.alpn = m_session.alpn_selected();
                if (m_sni.empty()) m_sni = m_session.requested_server_name();
                m_active->metrics()->on_handshake_completed(m_metrics);
            } catch (...) {}

            if (m_on_complete) { try { m_on_complete(); } catch (...) {} }
            return result;
        }

        case detail::HandshakeStatus::Failed:
        default: {
            HandshakeResult r = fail(classify(io), io.detail.empty() ? "Handshake failed" : io.detail);
            r.output = std::move(result.output);
            return r;
        }
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
    const VerifyMode mode = m_active->verify_mode();
    std::vector<std::vector<std::uint8_t>> ders;

    if (!m_session.peer_chain(ders) || ders.empty()) {
        if (mode == VerifyMode::FailIfNoPeer) {
            return fail(HandshakeError::CertificateRequired, "Peer did not present a certificate (FailIfNoPeer)");
        }

        if (m_role == HandshakeRole::Client && mode != VerifyMode::None) {
            return fail(HandshakeError::CertificateRequired, "Server did not present a certificate");
        }

        m_verify = VerifyResult::success();
        return { HandshakeState::Complete, HandshakeError::None, {}, {}, {} };
    }

    CertificateChain chain = CertificateChain::from_der_list(ders);

    if (chain.empty()) {
        return fail(HandshakeError::CertificateInvalid, "Peer certificate could not be parsed");
    }

    try {
        if (mode != VerifyMode::None) {
            std::shared_ptr<CertificateVerifier> v = m_verifier ? m_verifier : m_active->verifier();

            if (v) {
                m_verify = v->verify(verification_hostname(), chain.leaf(), &chain);
            } else {
                CertificateVerifier fallback;
                fallback.set_ca_store_path(m_active->ca_store_path());
                fallback.set_check_revocation(m_active->check_revocation());
                fallback.set_crl_files(m_active->crl_files());
                m_verify = fallback.verify(verification_hostname(), chain.leaf(), &chain);
            }

            if (!m_verify.passed) return apply(m_verify);
        } else {
            m_verify = VerifyResult::success();
        }

        if (m_role == HandshakeRole::Client) {
            if (m_active->request_ocsp()) {
                const std::vector<std::uint8_t> staple = m_session.ocsp_response();
                OCSPVerifier ocsp;
                ocsp.set_ca_store_path(m_active->ca_store_path());
                ocsp.set_require_staple(m_active->require_ocsp());

                if (staple.empty()) {
                    if (m_active->require_ocsp()) {
                        m_verify = VerifyResult::failure(VerifyError::RevocationUnknown, 0, "Server did not staple an OCSP response");
                        return apply(m_verify);
                    }
                } else {
                    const VerifyResult r = ocsp.to_verify_result(ocsp.check(staple, chain));
                    if (!r.passed) { m_verify = r; return apply(m_verify); }
                }
            }

            auto pins = m_active->pins();

            if (pins && !m_hostname.empty() && !pins->check(m_hostname, chain)) {
                m_verify = VerifyResult::failure(VerifyError::PinViolation, 0, "Certificate public key does not match any pin for \"" + m_hostname + "\"");
                return apply(m_verify);
            }
        }

        auto cb = m_active->peer_certificate_callback();

        if (cb && !cb(chain)) {
            m_verify = VerifyResult::failure(VerifyError::Rejected, 0, "Peer certificate rejected by callback");
            return apply(m_verify);
        }
    } catch (...) {
        return fail(HandshakeError::InternalError, "Certificate verification threw");
    }

    return { HandshakeState::Complete, HandshakeError::None, {}, {}, {} };
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
    try { m_active->metrics()->on_handshake_failed(m_verify.error); } catch (...) {}

    if (m_on_failed) {
        try { m_on_failed(err, msg); } catch (...) {}
    }

    return { HandshakeState::Failed, err, msg, {}, {} };
}

} // namespace security
} // namespace networking
} // namespace fizmo
