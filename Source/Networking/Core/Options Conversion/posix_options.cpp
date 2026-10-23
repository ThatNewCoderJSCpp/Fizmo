#include "fizmo_library.hpp"
#include "posix_options.hpp"

namespace fizmo {
namespace networking {
namespace core {

#if defined(OS_POSIX)
auto LinuxOptions::resolve(OptionCode code) noexcept -> NativeOption {
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
    #if defined(TCP_KEEPIDLE)
        case OptionCode::KeepAliveIdle:     return NativeOption(IPPROTO_TCP,  TCP_KEEPIDLE);
    #elif defined(TCP_KEEPALIVE)
        case OptionCode::KeepAliveIdle:     return NativeOption(IPPROTO_TCP,  TCP_KEEPALIVE);
    #else
        case OptionCode::KeepAliveIdle:     return NativeOption();
    #endif
    #ifdef TCP_KEEPINTVL
        case OptionCode::KeepAliveInterval: return NativeOption(IPPROTO_TCP,  TCP_KEEPINTVL);
    #else
        case OptionCode::KeepAliveInterval: return NativeOption();
    #endif
    #ifdef TCP_KEEPCNT
        case OptionCode::KeepAliveCount:    return NativeOption(IPPROTO_TCP,  TCP_KEEPCNT);
    #else
        case OptionCode::KeepAliveCount:    return NativeOption();
    #endif
        case OptionCode::TypeOfService:     return NativeOption(IPPROTO_IP,   IP_TOS);
        case OptionCode::PendingError:      return NativeOption(SOL_SOCKET,   SO_ERROR);
    #if defined(IP_PKTINFO)
        case OptionCode::ReceivePacketInfo: return NativeOption(IPPROTO_IP,   IP_PKTINFO);
    #elif defined(IP_RECVPKTINFO)
        case OptionCode::ReceivePacketInfo: return NativeOption(IPPROTO_IP,   IP_RECVPKTINFO);
    #else
        case OptionCode::ReceivePacketInfo: return NativeOption();
    #endif
        case OptionCode::OOBInline:         return NativeOption(SOL_SOCKET,   SO_OOBINLINE);
    #ifdef SO_BINDTODEVICE
        case OptionCode::BindToDevice:      return NativeOption(SOL_SOCKET,   SO_BINDTODEVICE);
    #else
        case OptionCode::BindToDevice:      return NativeOption();
    #endif
    #ifdef SO_PRIORITY
        case OptionCode::Priority:          return NativeOption(SOL_SOCKET,   SO_PRIORITY);
    #else
        case OptionCode::Priority:          return NativeOption();
    #endif
    #ifdef TCP_DEFER_ACCEPT
        case OptionCode::DeferAccept:       return NativeOption(IPPROTO_TCP,  TCP_DEFER_ACCEPT);
    #else
        case OptionCode::DeferAccept:       return NativeOption();
    #endif
    #ifdef TCP_QUICKACK
        case OptionCode::QuickAck:          return NativeOption(IPPROTO_TCP,  TCP_QUICKACK);
    #else
        case OptionCode::QuickAck:          return NativeOption();
    #endif
    #ifdef IP_FREEBIND
        case OptionCode::FreeBind:          return NativeOption(IPPROTO_IP,   IP_FREEBIND);
    #else
        case OptionCode::FreeBind:          return NativeOption();
    #endif
    #ifdef IP_MTU_DISCOVER
        case OptionCode::PathMTUDiscover:   return NativeOption(IPPROTO_IP,   IP_MTU_DISCOVER);
    #else
        case OptionCode::PathMTUDiscover:   return NativeOption();
    #endif
    #ifdef IP_TRANSPARENT
        case OptionCode::TransparentProxy:  return NativeOption(IPPROTO_IP,   IP_TRANSPARENT);
    #else
        case OptionCode::TransparentProxy:  return NativeOption();
    #endif
        default:                            return NativeOption();
    }
}
#endif

#if defined(OS_POSIX)
auto LinuxOptions::get_supported_options() noexcept -> std::vector<OptionCode> {
    static const OptionCode candidates[] = {
    #define FIZMO_CODE(name, cat, type) OptionCode::name,
        FIZMO_ALL_OPTIONS(FIZMO_CODE)
    #undef FIZMO_CODE
    };

    std::vector<OptionCode> result;
    for (auto code : candidates) { if (is_supported(code)) { result.push_back(code); } }
    return result;
}
#endif

} // namespace core
} // namespace networking
} // namespace fizmo
