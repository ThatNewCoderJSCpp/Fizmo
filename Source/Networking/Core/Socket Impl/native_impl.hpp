#ifndef FIZMO_NATIVE_IMPL_HPP
#define FIZMO_NATIVE_IMPL_HPP

#include "socket_impl_base.hpp"

#ifdef OS_WINDOWS
    #include "winsock_impl.hpp"
#else
    #include "posix_impl.hpp"
#endif

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

#ifdef OS_WINDOWS
    using NativeSocketImpl = WinsockImpl;
#else
    using NativeSocketImpl = PosixImpl;
#endif

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_IMPL_HPP