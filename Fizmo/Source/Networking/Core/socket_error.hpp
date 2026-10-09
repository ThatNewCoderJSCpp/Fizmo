#ifndef FIZMO_SOCKET_ERROR_HPP
#define FIZMO_SOCKET_ERROR_HPP

#include <string>
#include "../../Basic/basic_includes.hpp"

#ifdef OS_LINUX
#endif

namespace fizmo {
namespace networking {
namespace core {

enum class ErrorCode {
    Success = 0,
    Timeout,
    NetworkUnreachable,
    ConnectionRefused,
    AddressInUse,
    InvalidAddress,
    NotConnected,
    ConnectionReset,
    HostUnreachable,
    AccessDenied,
    OutOfMemory,
    SocketClosed,
    WouldBlock,
    InvalidArgument,
    BufferOverflow,
    MessageTooLarge,
    OperationInterrupted,
    ProtocolError,
    AddressFamilyNotSupported,
    SystemNotReady,
    AlreadyConnected,
    RemoteHostClosed,
    UnknownError,
    InProgress,
    NetworkDown,
    SSLError,
    ServerCertificateError,
    TooManyConnections, 
    SocketTypeNotSupported,
    Aborted,
    NameResolutionFailed,
    OperationNotSupported,
    ServiceUnavailable
};

enum class ErrorSeverity {
    Success = 0,    
    Info,       
    Warning, // Operation may have succeeded partially
    Error,   // Operation failed but recoverable
    Critical // Socket should be closed
};

class SocketError {
private:
    ErrorCode m_code;
    std::string m_message;
    ErrorSeverity m_severity;
    int m_native_error_code;
    std::string m_operation;

public:
    SocketError(ErrorCode code = ErrorCode::Success, const std::string& message = "", int native_error_code = 0, const std::string& operation = "") 
        noexcept;

public:
    static ErrorSeverity get_severity_for_code(ErrorCode code);

    static std::string code_to_string(ErrorCode code) noexcept;

    static std::string severity_to_string(ErrorSeverity severity) noexcept;

    static bool code_is_success(ErrorCode code) noexcept { return code == ErrorCode::Success; }
    static bool error_is_critical(ErrorSeverity severity) noexcept { return severity == ErrorSeverity::Critical; }
    static bool error_is_warning(ErrorSeverity severity) noexcept { return severity == ErrorSeverity::Warning; }
    static bool error_is_info(ErrorSeverity severity) noexcept { return severity == ErrorSeverity::Info; }
    static bool error_is_recoverable(ErrorSeverity severity) noexcept { return severity != ErrorSeverity::Critical; }
    
    static bool code_is_temporary(ErrorCode code) noexcept;

public:
    ErrorCode code() const noexcept { return m_code; }
    const std::string& message() const noexcept { return m_message; }
    ErrorSeverity severity() const noexcept { return m_severity; }
    int native_error_code() const noexcept { return m_native_error_code; }
    const std::string& operation() const noexcept { return m_operation; }

public:
    operator bool() const noexcept { return is_success(); }
    bool operator!() const noexcept { return is_error(); }
    bool operator==(const SocketError& other) const noexcept { return m_code == other.m_code; }
    bool operator!=(const SocketError& other) const noexcept { return m_code != other.m_code; }

public:
    bool is_success() const noexcept { return m_code == ErrorCode::Success; }
    bool is_error() const noexcept { return m_code != ErrorCode::Success; }
    bool is_critical() const noexcept { return m_severity == ErrorSeverity::Critical; }
    bool is_warning() const noexcept { return m_severity == ErrorSeverity::Warning; }
    bool is_info() const noexcept { return m_severity == ErrorSeverity::Info; }
    bool is_recoverable() const noexcept { return m_severity != ErrorSeverity::Critical; }
    bool is_temporary() const noexcept { return code_is_temporary(m_code); }

public:
    std::string to_string() const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const SocketError& error) {
        os << error.to_string();
        return os;
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_ERROR_HPP