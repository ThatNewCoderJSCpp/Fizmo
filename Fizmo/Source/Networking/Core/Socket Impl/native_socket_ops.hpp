#ifndef FIZMO_NATIVE_SOCKET_OPS_HPP
#define FIZMO_NATIVE_SOCKET_OPS_HPP

#include "native_impl.hpp"
#include "../Error Conversion/native_errors.hpp"
#include <cstring>
#include <memory>

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

namespace sockops {

inline constexpr int kSockRaw       = SOCK_RAW;
inline constexpr int kProtocolICMP  = IPPROTO_ICMP;
inline constexpr int kProtocolICMPv6= IPPROTO_ICMPV6;
inline constexpr int kProtocolRaw   = IPPROTO_RAW;

inline int address_family_value(AddressFamily family) noexcept {
    return (family == AddressFamily::IPv6) ? AF_INET6 : AF_INET;
}

inline native_handle_t create_raw_handle(AddressFamily family, int protocol) noexcept {
    return ::socket(address_family_value(family), kSockRaw, protocol);
}

 std::unique_ptr<SocketImplBase> adopt(
    native_handle_t handle,
    SocketType type,
    AddressFamily family
) noexcept;

inline native_handle_t handle_of(SocketImplBase* impl) noexcept {
    if (!impl) return kInvalidHandle;
    return impl->native_handle();
}

 int peek(SocketImplBase* impl, void* buffer, std::size_t length) noexcept;

 int peek_from(
    SocketImplBase* impl,
    void* buffer,
    std::size_t length,
    NetworkAddress& source
) noexcept;

 bool dissolve_association(SocketImplBase* impl) noexcept;

} // namespace sockops
} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_SOCKET_OPS_HPP