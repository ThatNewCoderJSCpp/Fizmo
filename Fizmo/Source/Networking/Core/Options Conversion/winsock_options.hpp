#ifndef FIZMO_WINSOCK_OPTIONS_HPP
#define FIZMO_WINSOCK_OPTIONS_HPP

#include "socket_option.hpp"

#ifdef OS_WINDOWS

namespace fizmo {
namespace networking {
namespace core {

class WindowsOptions {
public:
    static NativeOption resolve(OptionCode code) noexcept {
        switch (code) {
            // Common options
            case OptionCode::ReuseAddress:      
            case OptionCode::ReusePort:         
                return NativeOption(SOL_SOCKET, SO_REUSEADDR); // Windows maps ReusePort to ReuseAddr
            case OptionCode::TCPNoDelay:        return NativeOption(IPPROTO_TCP,  TCP_NODELAY);
            case OptionCode::KeepAlive:         return NativeOption(SOL_SOCKET,   SO_KEEPALIVE);
            case OptionCode::Linger:            return NativeOption(SOL_SOCKET,   SO_LINGER);
            case OptionCode::SendBufferSize:    return NativeOption(SOL_SOCKET,   SO_SNDBUF);
            case OptionCode::ReceiveBufferSize: return NativeOption(SOL_SOCKET,   SO_RCVBUF);
            case OptionCode::Broadcast:         return NativeOption(SOL_SOCKET,   SO_BROADCAST);
            case OptionCode::ReceiveTimeout:    return NativeOption(SOL_SOCKET,   SO_RCVTIMEO);
            case OptionCode::SendTimeout:       return NativeOption(SOL_SOCKET,   SO_SNDTIMEO);
            case OptionCode::IPv6Only:          return NativeOption(IPPROTO_IPV6, IPV6_V6ONLY);
            case OptionCode::TTL:               return NativeOption(IPPROTO_IP,   IP_TTL);
            case OptionCode::TCPFastOpen:       return NativeOption(IPPROTO_TCP,  TCP_FASTOPEN);
            case OptionCode::KeepAliveIdle:     
            case OptionCode::KeepAliveInterval: 
                return NativeOption();  // Via SIO_KEEPALIVE_VALS ioctl
            case OptionCode::KeepAliveCount:    return NativeOption(IPPROTO_TCP,  TCP_KEEPCNT); // Win10 1709+
            case OptionCode::TypeOfService:     return NativeOption(IPPROTO_IP,   IP_TOS);
            case OptionCode::PendingError:      return NativeOption(SOL_SOCKET,   SO_ERROR);
            case OptionCode::ReceivePacketInfo: return NativeOption(IPPROTO_IP,   IP_PKTINFO);
            case OptionCode::OOBInline:         return NativeOption(SOL_SOCKET,   SO_OOBINLINE);

            // Windows-specific options
            case OptionCode::ExclusiveAddress:  return NativeOption(SOL_SOCKET,   SO_EXCLUSIVEADDRUSE);
            case OptionCode::DontFragment:      return NativeOption(IPPROTO_IP,   IP_DONTFRAGMENT);

            default:                            return NativeOption();
        }
    }

    static bool is_supported(OptionCode code) noexcept { return resolve(code).supported; }

    static std::vector<OptionCode> get_supported_options() noexcept {
        static const OptionCode candidates[] = {
        #define FIZMO_CODE(name, cat, type) OptionCode::name,
            FIZMO_ALL_OPTIONS(FIZMO_CODE)
        #undef FIZMO_CODE
        };

        std::vector<OptionCode> result;
        for (auto code : candidates) { if (is_supported(code)) { result.push_back(code); } }
        return result;
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // OS_WINDOWS
#endif // FIZMO_WINSOCK_OPTIONS_HPP