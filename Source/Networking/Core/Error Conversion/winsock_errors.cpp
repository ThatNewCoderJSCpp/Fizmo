#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "winsock_errors.hpp"

namespace fizmo {
namespace networking {
namespace core {

#if defined(OS_WINDOWS)
auto WinsockErrorConverter::convert(int win_error, const std::string& operation) noexcept -> SocketError {
    if (win_error == 0) { return SocketError(ErrorCode::Success, "Operation completed successfully", 0, operation); }
    ErrorCode code;
    std::string message = get_error_message(win_error);
        
    switch (win_error) {
        case WSAETIMEDOUT:
            code = ErrorCode::Timeout;
            break;
        case WSAENETUNREACH:
            code = ErrorCode::NetworkUnreachable;
            break;
        case WSAECONNREFUSED:
            code = ErrorCode::ConnectionRefused;
            break;
        case WSAEADDRINUSE:
            code = ErrorCode::AddressInUse;
            break;
        case WSAEADDRNOTAVAIL:
            code = ErrorCode::InvalidAddress;
            break;
        case WSAENOTCONN:
            code = ErrorCode::NotConnected;
            break;
        case WSAECONNRESET:
            code = ErrorCode::ConnectionReset;
            break;
        case WSAEHOSTUNREACH:
            code = ErrorCode::HostUnreachable;
            break;
        case WSAEACCES:
            code = ErrorCode::AccessDenied;
            break;
        case WSA_NOT_ENOUGH_MEMORY:
        case WSAENOBUFS:
            code = ErrorCode::OutOfMemory;
            break;
        case WSAESHUTDOWN:
            code = ErrorCode::SocketClosed;
            break;
        case WSAEWOULDBLOCK:
            code = ErrorCode::WouldBlock;
            break;
        case WSAEINPROGRESS:
        case WSAEALREADY:
            code = ErrorCode::InProgress;
            break;
        case WSAEINVAL:
        case WSAEFAULT:
            code = ErrorCode::InvalidArgument;
            break;
        case WSAEMSGSIZE:
            code = ErrorCode::MessageTooLarge;
            break;
        case WSAEINTR:
            code = ErrorCode::OperationInterrupted;
            break;
        case WSAEPROTONOSUPPORT:
        case WSAEPROTOTYPE:
            code = ErrorCode::ProtocolError;
            break;
        case WSAESOCKTNOSUPPORT:
            code = ErrorCode::SocketTypeNotSupported;
            break;
        case WSAEAFNOSUPPORT:
            code = ErrorCode::AddressFamilyNotSupported;
            break;
        case WSASYSNOTREADY:
            code = ErrorCode::SystemNotReady;
            break;
        case WSAEISCONN:
            code = ErrorCode::AlreadyConnected;
            break;
        case WSAECONNABORTED:
            code = ErrorCode::Aborted;
            break;
        case WSAHOST_NOT_FOUND:
        case WSANO_DATA:
        case WSATRY_AGAIN:
        case WSANO_RECOVERY:
            code = ErrorCode::NameResolutionFailed;
            break;
        case WSAEOPNOTSUPP:
        case WSAENOPROTOOPT:
            code = ErrorCode::OperationNotSupported;
            break;
        case WSAENETDOWN:
            code = ErrorCode::NetworkDown;
            break;
        case WSASERVICE_NOT_FOUND:
            code = ErrorCode::ServiceUnavailable;
            break;
        case WSAETOOMANYREFS:
        case WSAEPROCLIM:
        case WSAEMFILE:
            code = ErrorCode::TooManyConnections;
            break;
        case WSAECANCELLED:
            code = ErrorCode::Aborted;
            break;
        case WSAEREMOTE:
            code = ErrorCode::RemoteHostClosed;
            break;
        default:
            code = ErrorCode::UnknownError;
            break;
    }
        
    return SocketError(code, message, win_error, operation);
}
#endif

#if defined(OS_WINDOWS)
auto WinsockErrorConverter::get_last_error(const std::string& operation) noexcept -> SocketError { return convert(WSAGetLastError(), operation); }
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::would_block() noexcept {
    int error = WSAGetLastError();
    return (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS);
}
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::is_connection_error() noexcept {
    int error = WSAGetLastError();
    return (error == WSAECONNREFUSED || error == WSAETIMEDOUT || error == WSAENETUNREACH || error == WSAEHOSTUNREACH);
}
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::is_temporary_error() noexcept {
    int error = WSAGetLastError();
    return (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINTR || error == WSAETIMEDOUT);
}
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::is_critical_error() noexcept {
    int error = WSAGetLastError();
    return (error == WSAECONNRESET || error == WSAENETDOWN || error == WSAECONNABORTED || error == WSAESHUTDOWN);
}
#endif

#if defined(OS_WINDOWS)
std::string WinsockErrorConverter::get_error_message(int error_code) noexcept {
    char* msg_buf = nullptr;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msg_buf,
        0,
        NULL
    );
        
    std::string message = msg_buf ? msg_buf : "Unknown error";
        
    if (!message.empty()) {
        size_t end = message.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) { message = message.substr(0, end + 1); }
    }
        
    if (msg_buf) { LocalFree(msg_buf); }
    return message;
}
#endif

#if defined(OS_WINDOWS)
auto WinsockErrorConverter::convert_ssl_error(SECURITY_STATUS sec_status, const std::string& operation) noexcept -> SocketError {
    std::string message;
    ErrorCode code;
        
    switch (sec_status) {
        case SEC_E_OK:
            return SocketError(ErrorCode::Success, "SSL operation completed successfully", sec_status, operation);
                
        case SEC_E_CERT_EXPIRED:
            message = "The SSL certificate has expired";
            code = ErrorCode::ServerCertificateError;
            break;
                
        case SEC_E_WRONG_PRINCIPAL:
            message = "The SSL certificate name doesn't match the host name";
            code = ErrorCode::ServerCertificateError;
            break;
                
        case SEC_E_UNTRUSTED_ROOT:
            message = "The SSL certificate was issued by an untrusted certificate authority";
            code = ErrorCode::ServerCertificateError;
            break;
                
        case SEC_E_CERT_UNKNOWN:
            message = "Unknown SSL certificate validation error";
            code = ErrorCode::ServerCertificateError;
            break;
                
        case SEC_E_ILLEGAL_MESSAGE:
        case SEC_E_INVALID_TOKEN:
            message = "Invalid SSL message received";
            code = ErrorCode::ProtocolError;
            break;
                
        case SEC_E_ALGORITHM_MISMATCH:
            message = "No compatible SSL encryption algorithm found";
            code = ErrorCode::SSLError;
            break;
                
        case SEC_E_INCOMPLETE_MESSAGE:
            message = "Incomplete SSL message received";
            code = ErrorCode::WouldBlock; 
            break;
                
        case SEC_E_INSUFFICIENT_MEMORY:
            message = "Insufficient memory for SSL operation";
            code = ErrorCode::OutOfMemory;
            break;
                
        case SEC_E_INTERNAL_ERROR:
            message = "Internal SSL error";
            code = ErrorCode::SSLError;
            break;
                
        case SEC_E_CONTEXT_EXPIRED:
            message = "SSL connection context has expired";
            code = ErrorCode::ConnectionReset;
            break;
                
        case SEC_E_NO_CREDENTIALS:
        case SEC_E_INCOMPLETE_CREDENTIALS:
            message = "SSL credentials not available or incomplete";
            code = ErrorCode::AccessDenied;
            break;
                
        case SEC_E_UNSUPPORTED_FUNCTION:
            message = "Requested SSL functionality is not supported";
            code = ErrorCode::OperationNotSupported;
            break;
                
        case SEC_I_CONTINUE_NEEDED:
            message = "SSL handshake needs to continue";
            code = ErrorCode::InProgress;
            break;
                
        case SEC_I_RENEGOTIATE:
            message = "SSL connection needs to be renegotiated";
            code = ErrorCode::InProgress;
            break;
                
        default:
            message = "Unknown SSL error";
            code = ErrorCode::SSLError;
            break;
    }
        
    std::string sys_message = get_error_message(sec_status);
    if (!sys_message.empty() && sys_message != "Unknown error") { message += ": " + sys_message; }
    return SocketError(code, message, sec_status, operation);
}
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::is_certificate_error(SECURITY_STATUS status) noexcept {
    return (status == SEC_E_CERT_EXPIRED || status == SEC_E_WRONG_PRINCIPAL || status == SEC_E_UNTRUSTED_ROOT || status == SEC_E_CERT_UNKNOWN);
}
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::is_network_unreachable() noexcept { return WSAGetLastError() == WSAENETUNREACH; }
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::is_host_unreachable() noexcept { return WSAGetLastError() == WSAEHOSTUNREACH; }
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::is_connection_refused() noexcept { return WSAGetLastError() == WSAECONNREFUSED; }
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::is_address_in_use() noexcept { return WSAGetLastError() == WSAEADDRINUSE; }
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::ssl_needs_more_data() noexcept { 
    SECURITY_STATUS status = static_cast<SECURITY_STATUS>(GetLastError());
    return status == SEC_E_INCOMPLETE_MESSAGE || status == SEC_I_CONTINUE_NEEDED;
}
#endif

#if defined(OS_WINDOWS)
bool WinsockErrorConverter::ssl_needs_renegotiation() noexcept { return static_cast<SECURITY_STATUS>(GetLastError()) == SEC_I_RENEGOTIATE; }
#endif

} // namespace core
} // namespace networking
} // namespace fizmo
