#ifndef FIZMO_SOCKET_ERROR_HPP
#define FIZMO_SOCKET_ERROR_HPP

#include <string>
#include "../../Basic/basic_includes.hpp"

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
        noexcept : m_code(code), m_message(message), m_severity(get_severity_for_code(code)), m_native_error_code(native_error_code), m_operation(operation) {}

public:
    static ErrorSeverity get_severity_for_code(ErrorCode code) {
        static const std::map<ErrorCode, ErrorSeverity> severity_map = {
            {ErrorCode::Success, ErrorSeverity::Success},
            {ErrorCode::WouldBlock, ErrorSeverity::Info},
            {ErrorCode::InProgress, ErrorSeverity::Info},
            {ErrorCode::BufferOverflow, ErrorSeverity::Warning},
            {ErrorCode::OperationInterrupted, ErrorSeverity::Warning},
            {ErrorCode::Timeout, ErrorSeverity::Error},
            {ErrorCode::ConnectionRefused, ErrorSeverity::Error},
            {ErrorCode::NetworkUnreachable, ErrorSeverity::Error},
            {ErrorCode::HostUnreachable, ErrorSeverity::Error},
            {ErrorCode::AddressInUse, ErrorSeverity::Error},
            {ErrorCode::InvalidAddress, ErrorSeverity::Error},
            {ErrorCode::NotConnected, ErrorSeverity::Error},
            {ErrorCode::AccessDenied, ErrorSeverity::Error},
            {ErrorCode::OutOfMemory, ErrorSeverity::Error},
            {ErrorCode::InvalidArgument, ErrorSeverity::Error},
            {ErrorCode::MessageTooLarge, ErrorSeverity::Error},
            {ErrorCode::ProtocolError, ErrorSeverity::Error},
            {ErrorCode::AddressFamilyNotSupported, ErrorSeverity::Error},
            {ErrorCode::SystemNotReady, ErrorSeverity::Error},
            {ErrorCode::AlreadyConnected, ErrorSeverity::Error},
            {ErrorCode::UnknownError, ErrorSeverity::Error},
            {ErrorCode::TooManyConnections, ErrorSeverity::Error},
            {ErrorCode::SocketTypeNotSupported, ErrorSeverity::Error},
            {ErrorCode::Aborted, ErrorSeverity::Error},
            {ErrorCode::NameResolutionFailed, ErrorSeverity::Error},
            {ErrorCode::OperationNotSupported, ErrorSeverity::Error},
            {ErrorCode::ServiceUnavailable, ErrorSeverity::Error},
            {ErrorCode::ConnectionReset, ErrorSeverity::Critical},
            {ErrorCode::SocketClosed, ErrorSeverity::Critical},
            {ErrorCode::RemoteHostClosed, ErrorSeverity::Critical},
            {ErrorCode::NetworkDown, ErrorSeverity::Critical},
            {ErrorCode::SSLError, ErrorSeverity::Critical},
            {ErrorCode::ServerCertificateError, ErrorSeverity::Critical}
        };
        
        auto it = severity_map.find(code);
        if (it != severity_map.end()) { return it->second; }
        return code == ErrorCode::Success ? ErrorSeverity::Success : ErrorSeverity::Error;
    }

    static std::string code_to_string(ErrorCode code) noexcept {
        switch (code) {
            case ErrorCode::Success: return "Success";
            case ErrorCode::Timeout: return "Timeout";
            case ErrorCode::NetworkUnreachable: return "NetworkUnreachable";
            case ErrorCode::ConnectionRefused: return "ConnectionRefused";
            case ErrorCode::AddressInUse: return "AddressInUse";
            case ErrorCode::InvalidAddress: return "InvalidAddress";
            case ErrorCode::NotConnected: return "NotConnected";
            case ErrorCode::ConnectionReset: return "ConnectionReset";
            case ErrorCode::HostUnreachable: return "HostUnreachable";
            case ErrorCode::AccessDenied: return "AccessDenied";
            case ErrorCode::OutOfMemory: return "OutOfMemory";
            case ErrorCode::SocketClosed: return "SocketClosed";
            case ErrorCode::WouldBlock: return "WouldBlock";
            case ErrorCode::InvalidArgument: return "InvalidArgument";
            case ErrorCode::BufferOverflow: return "BufferOverflow";
            case ErrorCode::MessageTooLarge: return "MessageTooLarge";
            case ErrorCode::OperationInterrupted: return "OperationInterrupted";
            case ErrorCode::ProtocolError: return "ProtocolError";
            case ErrorCode::AddressFamilyNotSupported: return "AddressFamilyNotSupported";
            case ErrorCode::SystemNotReady: return "SystemNotReady";
            case ErrorCode::AlreadyConnected: return "AlreadyConnected";
            case ErrorCode::RemoteHostClosed: return "RemoteHostClosed";
            case ErrorCode::UnknownError: return "UnknownError";
            case ErrorCode::InProgress: return "InProgress";
            case ErrorCode::NetworkDown: return "NetworkDown";
            case ErrorCode::SSLError: return "SSLError";
            case ErrorCode::ServerCertificateError: return "ServerCertificateError";
            case ErrorCode::TooManyConnections: return "TooManyConnections";
            case ErrorCode::SocketTypeNotSupported: return "SocketTypeNotSupported";
            case ErrorCode::Aborted: return "Aborted";
            case ErrorCode::NameResolutionFailed: return "NameResolutionFailed";
            case ErrorCode::OperationNotSupported: return "OperationNotSupported";
            case ErrorCode::ServiceUnavailable: return "ServiceUnavailable";
            default: return "UndefinedError";
        }
    }

    static std::string severity_to_string(ErrorSeverity severity) noexcept {
        switch (severity) {
            case ErrorSeverity::Success: return "Success";
            case ErrorSeverity::Info: return "Info";
            case ErrorSeverity::Warning: return "Warning";
            case ErrorSeverity::Error: return "Error";
            case ErrorSeverity::Critical: return "Critical";
            default: return "Unknown";
        }
    }

    static bool code_is_success(ErrorCode code) noexcept { return code == ErrorCode::Success; }
    static bool error_is_critical(ErrorSeverity severity) noexcept { return severity == ErrorSeverity::Critical; }
    static bool error_is_warning(ErrorSeverity severity) noexcept { return severity == ErrorSeverity::Warning; }
    static bool error_is_info(ErrorSeverity severity) noexcept { return severity == ErrorSeverity::Info; }
    static bool error_is_recoverable(ErrorSeverity severity) noexcept { return severity != ErrorSeverity::Critical; }
    
    static bool code_is_temporary(ErrorCode code) noexcept {
        return code == ErrorCode::WouldBlock || code == ErrorCode::InProgress || code == ErrorCode::Timeout || code == ErrorCode::OperationInterrupted;
    }

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
    std::string to_string() const noexcept {
        std::string result = code_to_string(m_code);
        if (!m_operation.empty()) { result += " [" + m_operation + "]"; }
        result += " (" + severity_to_string(m_severity) + ")";
        if (m_native_error_code != 0) { result += " (Native: " + std::to_string(m_native_error_code) + ")"; }
        if (!m_message.empty()) { result += ": " + m_message; }
        return result;
    }

    friend std::ostream& operator<<(std::ostream& os, const SocketError& error) {
        os << error.to_string();
        return os;
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_ERROR_HPP