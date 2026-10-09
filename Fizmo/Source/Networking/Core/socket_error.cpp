#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "socket_error.hpp"

namespace fizmo {
namespace networking {
namespace core {

SocketError::SocketError(ErrorCode code, const std::string& message, int native_error_code, const std::string& operation) noexcept : m_code(code), m_message(message), m_severity(get_severity_for_code(code)), m_native_error_code(native_error_code), m_operation(operation) {}

auto SocketError::get_severity_for_code(ErrorCode code) -> ErrorSeverity {
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

auto SocketError::code_to_string(ErrorCode code) noexcept -> std::string {
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

auto SocketError::severity_to_string(ErrorSeverity severity) noexcept -> std::string {
        switch (severity) {
            case ErrorSeverity::Success: return "Success";
            case ErrorSeverity::Info: return "Info";
            case ErrorSeverity::Warning: return "Warning";
            case ErrorSeverity::Error: return "Error";
            case ErrorSeverity::Critical: return "Critical";
            default: return "Unknown";
        }
    }

auto SocketError::code_is_temporary(ErrorCode code) noexcept -> bool {
        return code == ErrorCode::WouldBlock || code == ErrorCode::InProgress || code == ErrorCode::Timeout || code == ErrorCode::OperationInterrupted;
    }

auto SocketError::to_string() const noexcept -> std::string {
        std::string result = code_to_string(m_code);
        if (!m_operation.empty()) { result += " [" + m_operation + "]"; }
        result += " (" + severity_to_string(m_severity) + ")";
        if (m_native_error_code != 0) { result += " (Native: " + std::to_string(m_native_error_code) + ")"; }
        if (!m_message.empty()) { result += ": " + m_message; }
        return result;
    }

} // namespace core
} // namespace networking
} // namespace fizmo
