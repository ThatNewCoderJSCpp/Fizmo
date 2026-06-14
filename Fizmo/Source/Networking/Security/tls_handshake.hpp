#ifndef FIZMO_TLS_HANDSHAKE_HPP
#define FIZMO_TLS_HANDSHAKE_HPP

#include "tls_context.hpp"
#include "tls_record.hpp"
#include "certificate_verifier.hpp"
#include "../Core/socket_error.hpp"
#include "../../Basic/basic_includes.hpp"
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
    InProgress,             // ISC / ASC loop running
    NeedMoreData,           // waiting for peer bytes
    PeerCertificateCheck,   // handshake paused for cert verification
    Renegotiating,          // mid-connection renegotiation / key update
    Complete,               // keys are ready
    Failed                  // terminal error
};

inline const char* handshake_state_to_string(HandshakeState s) noexcept {
    switch (s) {
        case HandshakeState::NotStarted:            return "NotStarted";
        case HandshakeState::InProgress:            return "InProgress";
        case HandshakeState::NeedMoreData:          return "NeedMoreData";
        case HandshakeState::PeerCertificateCheck:  return "PeerCertificateCheck";
        case HandshakeState::Renegotiating:         return "Renegotiating";
        case HandshakeState::Complete:              return "Complete";
        case HandshakeState::Failed:                return "Failed";
        default:                                    return "Unknown";
    }
}

enum class HandshakeError : std::uint8_t {
    None = 0,
    InternalError,
    ProtocolError,
    CertificateRequired,    // FailIfNoPeer: peer sent no certificate
    CertificateInvalid,     // verification failed (expired, untrusted, etc.)
    HostnameMismatch,
    AlgorithmMismatch,
    Timeout,
    PeerRejected,           // peer sent a fatal alert
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
    HandshakeState             state       = HandshakeState::NotStarted;
    HandshakeError             error       = HandshakeError::None;
    std::string                detail;
    std::vector<std::uint8_t>  output;     
    std::vector<std::uint8_t>  extra;       

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
    TLSHandshake(TLSContext& ctx, HandshakeRole role, const std::string& hostname = {}) noexcept
        : m_ctx(ctx),
          m_role(role),
          m_hostname(hostname),
          m_state(HandshakeState::NotStarted)
    #ifdef OS_WINDOWS
        , m_ctx_established(false)
    #endif
    {
    #ifdef OS_WINDOWS
        std::memset(&m_sec_ctx, 0, sizeof(m_sec_ctx));
    #endif
    }

    ~TLSHandshake() noexcept { release(); }

    TLSHandshake(const TLSHandshake&)            = delete;
    TLSHandshake& operator=(const TLSHandshake&) = delete;
    TLSHandshake(TLSHandshake&&)                 = default;
    TLSHandshake& operator=(TLSHandshake&&)      = default;

    void set_on_complete(std::function<void()> cb)  { m_on_complete = std::move(cb); }
    void set_on_failed(std::function<void(HandshakeError, const std::string&)> cb)      { m_on_failed   = std::move(cb); }
    void set_verifier(std::shared_ptr<CertificateVerifier> v) { m_verifier = std::move(v); }

    HandshakeState state()    const noexcept { return m_state; }
    HandshakeRole  role()     const noexcept { return m_role; }
    bool is_complete()        const noexcept { return m_state == HandshakeState::Complete; }
    bool has_failed()         const noexcept { return m_state == HandshakeState::Failed; }
    const std::string& hostname() const noexcept { return m_hostname; }

#ifdef OS_WINDOWS
    const CtxtHandle& security_context() const noexcept { return m_sec_ctx; }
    bool context_established()           const noexcept { return m_ctx_established; }
#endif

    HandshakeResult start() noexcept {
        if (!m_ctx.is_initialized()) {
            return fail(HandshakeError::ContextNotInitialised, "TLSContext not initialised");
        }

        m_state = HandshakeState::InProgress;

    #ifdef OS_WINDOWS
        if (m_role == HandshakeRole::Client) {
            return client_step(nullptr, 0);
        }

        m_state = HandshakeState::NeedMoreData;
        return { HandshakeState::NeedMoreData, HandshakeError::None, {}, {}, {} };
    #else
        return fail(HandshakeError::InternalError, "Platform handshake not implemented");
    #endif
    }

    HandshakeResult step(const std::uint8_t* data, std::size_t len) noexcept {
        if (m_state == HandshakeState::Complete ||
            m_state == HandshakeState::Failed) {
            return { m_state, HandshakeError::None, {}, {}, {} };
        }

    #ifdef OS_WINDOWS
        if (m_role == HandshakeRole::Client) { return client_step(data, len); }
        return server_step(data, len);
    #else
        (void)data; (void)len;
        return fail(HandshakeError::InternalError, "Platform handshake not implemented");
    #endif
    }

    HandshakeResult step(const std::vector<std::uint8_t>& data) noexcept {
        return step(data.data(), data.size());
    }

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
    TLSContext&                           m_ctx;
    HandshakeRole                         m_role;
    std::string                           m_hostname;
    HandshakeState                        m_state;

    std::function<void()>                   m_on_complete;
    std::function<void(HandshakeError, const std::string&)>                     m_on_failed;
    std::shared_ptr<CertificateVerifier>  m_verifier;

#ifdef OS_WINDOWS
    CtxtHandle m_sec_ctx;
    bool       m_ctx_established;
#endif

    HandshakeResult fail(HandshakeError err, const std::string& msg = {}) noexcept {
        m_state = HandshakeState::Failed;

        if (m_on_failed) {
            try { m_on_failed(err, msg); } catch (...) {}
        }
        return { HandshakeState::Failed, err, msg, {}, {} };
    }
    
    HandshakeResult succeed(std::vector<std::uint8_t> output = {}, std::vector<std::uint8_t> extra  = {}) noexcept {
        m_state = HandshakeState::Complete;

        if (m_on_complete) {
            try { m_on_complete(); } catch (...) {}
        }

        return { HandshakeState::Complete, HandshakeError::None, {}, std::move(output), std::move(extra) };
    }

    void release() noexcept {
    #ifdef OS_WINDOWS
        if (m_ctx_established) {
            DeleteSecurityContext(&m_sec_ctx);
            std::memset(&m_sec_ctx, 0, sizeof(m_sec_ctx));
            m_ctx_established = false;
        }
    #endif
    }

#ifdef OS_WINDOWS
    DWORD client_context_flags() const noexcept {
        DWORD flags = ISC_REQ_SEQUENCE_DETECT
                    | ISC_REQ_REPLAY_DETECT
                    | ISC_REQ_CONFIDENTIALITY
                    | ISC_REQ_STREAM
                    | ISC_REQ_ALLOCATE_MEMORY;

        if (m_ctx.verify_mode() == VerifyMode::None) {
            flags |= ISC_REQ_MANUAL_CRED_VALIDATION;
        }

        return flags;
    }

    DWORD server_context_flags() const noexcept {
        DWORD flags = ASC_REQ_SEQUENCE_DETECT
                    | ASC_REQ_REPLAY_DETECT
                    | ASC_REQ_CONFIDENTIALITY
                    | ASC_REQ_STREAM
                    | ASC_REQ_ALLOCATE_MEMORY;

        if (m_ctx.verify_mode() == VerifyMode::FailIfNoPeer) {
            flags |= ASC_REQ_MUTUAL_AUTH;
        }

        return flags;
    }

    HandshakeResult client_step(const std::uint8_t* in_data, std::size_t in_len) noexcept {
        SecBuffer  in_bufs[2]  = {};
        in_bufs[0].BufferType  = SECBUFFER_TOKEN;
        in_bufs[0].pvBuffer    = const_cast<std::uint8_t*>(in_data);
        in_bufs[0].cbBuffer    = static_cast<ULONG>(in_len);
        in_bufs[1].BufferType  = SECBUFFER_EMPTY;
        SecBufferDesc in_desc  = {};
        in_desc.ulVersion      = SECBUFFER_VERSION;
        in_desc.cBuffers       = 2;
        in_desc.pBuffers       = in_bufs;
        SecBuffer  out_buf     = {};
        out_buf.BufferType     = SECBUFFER_TOKEN;
        SecBufferDesc out_desc = {};
        out_desc.ulVersion     = SECBUFFER_VERSION;
        out_desc.cBuffers      = 1;
        out_desc.pBuffers      = &out_buf;
        DWORD       out_flags  = 0;
        TimeStamp   expiry     = {};
        std::wstring wide_host = utf8_to_wide(m_hostname);
        SEC_WCHAR* target = wide_host.empty() ? nullptr : const_cast<SEC_WCHAR*>(wide_host.c_str());
        CredHandle cred = m_ctx.credential_handle();

        SECURITY_STATUS status = InitializeSecurityContextW(
            &cred,
            m_ctx_established ? &m_sec_ctx : nullptr,
            target,
            client_context_flags(),
            0,                          
            0,                          
            (in_data && in_len > 0) ? &in_desc : nullptr,
            0,                          
            m_ctx_established ? nullptr : &m_sec_ctx,
            &out_desc,
            &out_flags,
            &expiry
        );

        return process_isc_asc_result(status, out_buf, in_bufs[1]);
    }

    HandshakeResult server_step(const std::uint8_t* in_data, std::size_t in_len) noexcept {
        if (!in_data || in_len == 0) {
            m_state = HandshakeState::NeedMoreData;
            return { HandshakeState::NeedMoreData, HandshakeError::None, {}, {}, {} };
        }

        SecBuffer  in_bufs[2]  = {};
        in_bufs[0].BufferType  = SECBUFFER_TOKEN;
        in_bufs[0].pvBuffer    = const_cast<std::uint8_t*>(in_data);
        in_bufs[0].cbBuffer    = static_cast<ULONG>(in_len);
        in_bufs[1].BufferType  = SECBUFFER_EMPTY;
        SecBufferDesc in_desc  = {};
        in_desc.ulVersion      = SECBUFFER_VERSION;
        in_desc.cBuffers       = 2;
        in_desc.pBuffers       = in_bufs;
        SecBuffer  out_buf     = {};
        out_buf.BufferType     = SECBUFFER_TOKEN;
        SecBufferDesc out_desc = {};
        out_desc.ulVersion     = SECBUFFER_VERSION;
        out_desc.cBuffers      = 1;
        out_desc.pBuffers      = &out_buf;
        DWORD       out_flags  = 0;
        TimeStamp   expiry     = {};
        CredHandle cred = m_ctx.credential_handle();

        SECURITY_STATUS status = AcceptSecurityContext(
            &cred,
            m_ctx_established ? &m_sec_ctx : nullptr,
            &in_desc,
            server_context_flags(),
            0,
            m_ctx_established ? nullptr : &m_sec_ctx,
            &out_desc,
            &out_flags,
            &expiry
        );

        return process_isc_asc_result(status, out_buf, in_bufs[1]);
    }

    HandshakeResult process_isc_asc_result(
        SECURITY_STATUS status,
        SecBuffer& out_buf,
        SecBuffer& extra_buf
    ) noexcept {
        HandshakeResult result;

        if (out_buf.cbBuffer > 0 && out_buf.pvBuffer) {
            auto* p = static_cast<std::uint8_t*>(out_buf.pvBuffer);
            result.output.assign(p, p + out_buf.cbBuffer);
        }

        if (extra_buf.BufferType == SECBUFFER_EXTRA && extra_buf.cbBuffer > 0) {
            result.extra.resize(extra_buf.cbBuffer);
        }

        switch (status) {
            case SEC_E_OK: {
                m_ctx_established = true;
                HandshakeResult cert_result = verify_peer_certificate();

                if (cert_result.has_failed()) {
                    free_output(out_buf);
                    return cert_result;
                }

                result.state = HandshakeState::Complete;
                result.error = HandshakeError::None;
                free_output(out_buf);

                if (m_on_complete) {
                    try { m_on_complete(); } catch (...) {}
                }

                m_state = HandshakeState::Complete;
                return result;
            }

            case SEC_I_CONTINUE_NEEDED:
                m_ctx_established = true;
                m_state = HandshakeState::InProgress;
                result.state = HandshakeState::InProgress;
                free_output(out_buf);
                return result;

            case SEC_E_INCOMPLETE_MESSAGE:
                m_state = HandshakeState::NeedMoreData;
                result.state = HandshakeState::NeedMoreData;
                free_output(out_buf);
                return result;

            case SEC_I_COMPLETE_NEEDED:
            case SEC_I_COMPLETE_AND_CONTINUE: {
                CompleteAuthToken(
                    &m_sec_ctx,
                    nullptr   
                );

                m_ctx_established = true;

                if (status == SEC_I_COMPLETE_AND_CONTINUE) {
                    m_state = HandshakeState::InProgress;
                    result.state = HandshakeState::InProgress;
                } else {
                    HandshakeResult cert_result = verify_peer_certificate();

                    if (cert_result.has_failed()) {
                        free_output(out_buf);
                        return cert_result;
                    }

                    m_state = HandshakeState::Complete;
                    result.state = HandshakeState::Complete;
                    if (m_on_complete) { try { m_on_complete(); } catch (...) {} }
                }

                free_output(out_buf);
                return result;
            }

            case SEC_I_RENEGOTIATE:
                m_state = HandshakeState::Renegotiating;
                result.state = HandshakeState::Renegotiating;
                free_output(out_buf);
                return result;

            case SEC_E_CERT_EXPIRED:
            case SEC_E_CERT_UNKNOWN:
            case SEC_E_UNTRUSTED_ROOT:
                free_output(out_buf);
                return fail(HandshakeError::CertificateInvalid, "Certificate validation failed (0x" + to_hex(status) + ")");

            case SEC_E_WRONG_PRINCIPAL:
                free_output(out_buf);
                return fail(HandshakeError::HostnameMismatch, "Server name mismatch");

            case SEC_E_ALGORITHM_MISMATCH:
                free_output(out_buf);
                return fail(HandshakeError::AlgorithmMismatch, "No common cipher suite");

            case SEC_E_NO_CREDENTIALS:
            case SEC_E_INCOMPLETE_CREDENTIALS:
                free_output(out_buf);
                return fail(HandshakeError::CertificateRequired, "Credentials missing or incomplete");

            default:
                free_output(out_buf);
                return fail(HandshakeError::InternalError, "Schannel error 0x" + to_hex(status));
        }
    }

    HandshakeResult verify_peer_certificate() noexcept {
        VerifyMode mode = m_ctx.verify_mode();

        if (mode == VerifyMode::None) {
            return { HandshakeState::Complete, HandshakeError::None, {}, {}, {} };
        }

        PCCERT_CONTEXT peer_cert = nullptr;

        SECURITY_STATUS qs = QueryContextAttributes(
            &m_sec_ctx,
            SECPKG_ATTR_REMOTE_CERT_CONTEXT,
            &peer_cert
        );

        if (FAILED(qs) || !peer_cert) {
            // No peer certificate available.
            if (mode == VerifyMode::FailIfNoPeer) {
                return fail(
                    HandshakeError::CertificateRequired,
                    "Peer did not present a certificate (FailIfNoPeer)"
                );
            }
            
            if (m_role == HandshakeRole::Client) {
                return fail(HandshakeError::CertificateRequired, "Server did not present a certificate");
            }
            
            return { HandshakeState::Complete, HandshakeError::None, {}, {}, {} };
        }

        HandshakeResult result = { HandshakeState::Complete, HandshakeError::None, {}, {}, {} };

        if (m_verifier) {
            VerifyResult vr = m_verifier->verify(m_hostname, peer_cert);

            if (!vr.passed) {
                CertFreeCertificateContext(peer_cert);
                HandshakeError he = HandshakeError::CertificateInvalid;
                if (vr.error == VerifyError::HostnameMismatch) { he = HandshakeError::HostnameMismatch; }

                return fail(
                    he, 
                    "Certificate verification failed: " +
                    std::string(verify_error_to_string(vr.error)) +
                    (vr.detail.empty() ? "" : " — " + vr.detail)
                );
            }
        }

        CertFreeCertificateContext(peer_cert);
        return result;
    }

    static void free_output(SecBuffer& buf) noexcept {
        if (buf.pvBuffer) {
            FreeContextBuffer(buf.pvBuffer);
            buf.pvBuffer  = nullptr;
            buf.cbBuffer  = 0;
        }
    }

    static std::wstring utf8_to_wide(const std::string& utf8) noexcept {
        if (utf8.empty()) return {};

        int needed = MultiByteToWideChar(
            CP_UTF8, 
            MB_ERR_INVALID_CHARS,
            utf8.data(), 
            static_cast<int>(utf8.size()),
            nullptr, 
            0
        );

        if (needed <= 0) return {};
        std::wstring wide(static_cast<std::size_t>(needed), L'\0');

        int written = MultiByteToWideChar(
            CP_UTF8, 
            MB_ERR_INVALID_CHARS,
            utf8.data(), 
            static_cast<int>(utf8.size()),
            &wide[0], 
            needed
        );

        if (written <= 0) return {};
        wide.resize(static_cast<std::size_t>(written));
        return wide;
    }

    static std::string to_hex(SECURITY_STATUS val) {
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%08lX", static_cast<unsigned long>(static_cast<DWORD>(val)));
        return buf;
    }

#endif // OS_WINDOWS
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_TLS_HANDSHAKE_HPP