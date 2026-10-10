#ifndef FIZMO_NATIVE_ERRORS_HPP
#define FIZMO_NATIVE_ERRORS_HPP

#include "../socket_error.hpp"

#ifdef OS_WINDOWS
    #include "winsock_errors.hpp"
#else
    #include "posix_errors.hpp"
#endif

namespace fizmo {
namespace networking {
namespace core {

#ifdef OS_WINDOWS
    using NativeErrorConverter = WinsockErrorConverter;
#else
    using NativeErrorConverter = PosixErrorConverter;
#endif

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_ERRORS_HPP