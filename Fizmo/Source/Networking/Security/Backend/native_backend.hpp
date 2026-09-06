#ifndef FIZMO_NATIVE_TLS_BACKEND_HPP
#define FIZMO_NATIVE_TLS_BACKEND_HPP

#include "backend_base.hpp"
#include "pem_utils.hpp"

#ifdef OS_WINDOWS
    #include "schannel_backend.hpp"
#else
    #include "openssl_backend.hpp"
#endif

namespace fizmo {
namespace networking {
namespace security {
namespace detail {

#ifdef OS_WINDOWS
    using NativeCertificate = SchannelCertificate;
    using NativeCertOps     = SchannelCertOps;
    using NativeCredentials = SchannelCredentials;
    using NativeSession     = SchannelSession;
    inline constexpr const char* kBackendName = "Schannel";
#else
    using NativeCertificate = OpenSSLCertificate;
    using NativeCertOps     = OpenSSLCertOps;
    using NativeCredentials = OpenSSLCredentials;
    using NativeSession     = OpenSSLSession;
    inline constexpr const char* kBackendName = "OpenSSL";
#endif

} // namespace detail
} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_TLS_BACKEND_HPP