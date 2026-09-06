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

inline std::unique_ptr<SocketImplBase> adopt(
    native_handle_t handle,
    SocketType type,
    AddressFamily family
) noexcept {
    if (handle == kInvalidHandle) return nullptr;
    return std::unique_ptr<SocketImplBase>(new NativeSocketImpl(handle, type, family));
}

inline native_handle_t handle_of(SocketImplBase* impl) noexcept {
    if (!impl) return kInvalidHandle;
    return impl->native_handle();
}

inline int peek(SocketImplBase* impl, void* buffer, std::size_t length) noexcept {
    native_handle_t handle = handle_of(impl);
    if (handle == kInvalidHandle || !buffer) return -1;
    auto n = ::recv(handle, static_cast<char*>(buffer), static_cast<int>(length), MSG_PEEK);
    return (n < 0) ? -1 : static_cast<int>(n);
}

inline int peek_from(
    SocketImplBase* impl,
    void* buffer,
    std::size_t length,
    NetworkAddress& source
) noexcept {
    native_handle_t handle = handle_of(impl);
    if (handle == kInvalidHandle || !buffer) return -1;
    struct sockaddr_storage storage;
    std::memset(&storage, 0, sizeof(storage));
    socklen_type addr_len = static_cast<socklen_type>(sizeof(storage));

    auto n = ::recvfrom(
        handle,
        static_cast<char*>(buffer),
        static_cast<int>(length),
        MSG_PEEK,
        reinterpret_cast<struct sockaddr*>(&storage),
        &addr_len
    );

    if (n < 0) return -1;
    source = NativeSocketImpl::parse_sockaddr(storage);
    return static_cast<int>(n);
}

inline bool dissolve_association(SocketImplBase* impl) noexcept {
    native_handle_t handle = handle_of(impl);
    if (handle == kInvalidHandle) return false;
    struct sockaddr_storage storage;
    std::memset(&storage, 0, sizeof(storage));
    storage.ss_family = AF_UNSPEC;

    int result = ::connect(
        handle,
        reinterpret_cast<struct sockaddr*>(&storage),
        static_cast<socklen_type>(sizeof(storage))
    );

    if (result == 0) return true;
    return NativeErrorConverter::get_last_error("disconnect").code() == ErrorCode::AddressFamilyNotSupported;
}

} // namespace sockops
} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_SOCKET_OPS_HPP