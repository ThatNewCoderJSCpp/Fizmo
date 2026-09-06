#ifndef FIZMO_POSIX_OPTIONS_HPP
#define FIZMO_POSIX_OPTIONS_HPP

#include "socket_option.hpp"

#ifdef OS_LINUX

namespace fizmo {
namespace networking {
namespace core {

class LinuxOptions {
public:
    static NativeOption resolve(OptionCode code) noexcept {
        switch (code) {
            case OptionCode::ReuseAddress:      return NativeOption(SOL_SOCKET,   SO_REUSEADDR);
        #ifdef SO_REUSEPORT
            case OptionCode::ReusePort:         return NativeOption(SOL_SOCKET,   SO_REUSEPORT);
        #else
            case OptionCode::ReusePort:         return NativeOption();
        #endif
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
        #ifdef TCP_FASTOPEN
            case OptionCode::TCPFastOpen:       return NativeOption(IPPROTO_TCP,  TCP_FASTOPEN);
        #else
            case OptionCode::TCPFastOpen:       return NativeOption();
        #endif
            case OptionCode::KeepAliveIdle:     return NativeOption(IPPROTO_TCP,  TCP_KEEPIDLE);
            case OptionCode::KeepAliveInterval: return NativeOption(IPPROTO_TCP,  TCP_KEEPINTVL);
            case OptionCode::KeepAliveCount:    return NativeOption(IPPROTO_TCP,  TCP_KEEPCNT);
            case OptionCode::TypeOfService:     return NativeOption(IPPROTO_IP,   IP_TOS);
            case OptionCode::PendingError:      return NativeOption(SOL_SOCKET,   SO_ERROR);
            case OptionCode::ReceivePacketInfo: return NativeOption(IPPROTO_IP,   IP_PKTINFO);
            case OptionCode::OOBInline:         return NativeOption(SOL_SOCKET,   SO_OOBINLINE);
            case OptionCode::BindToDevice:      return NativeOption(SOL_SOCKET,   SO_BINDTODEVICE);
            case OptionCode::Priority:          return NativeOption(SOL_SOCKET,   SO_PRIORITY);
            case OptionCode::DeferAccept:       return NativeOption(IPPROTO_TCP,  TCP_DEFER_ACCEPT);
            case OptionCode::QuickAck:          return NativeOption(IPPROTO_TCP,  TCP_QUICKACK);
        #ifdef IP_FREEBIND
            case OptionCode::FreeBind:          return NativeOption(IPPROTO_IP,   IP_FREEBIND);
        #else
            case OptionCode::FreeBind:          return NativeOption();
        #endif
            case OptionCode::PathMTUDiscover:   return NativeOption(IPPROTO_IP,   IP_MTU_DISCOVER);
        #ifdef IP_TRANSPARENT
            case OptionCode::TransparentProxy:  return NativeOption(IPPROTO_IP,   IP_TRANSPARENT);
        #else
            case OptionCode::TransparentProxy:  return NativeOption();
        #endif
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

#endif // OS_LINUX
#endif // FIZMO_POSIX_OPTIONS_HPP