#ifndef FIZMO_WINSOCK_OPTIONS_HPP
#define FIZMO_WINSOCK_OPTIONS_HPP

#include "socket_option.hpp"

#ifdef OS_WINDOWS

#ifndef TCP_FASTOPEN
    #define TCP_FASTOPEN 15
#endif
#ifndef TCP_KEEPCNT
    #define TCP_KEEPCNT 16
#endif

namespace fizmo {
namespace networking {
namespace core {

class WindowsOptions {
public:
    static NativeOption resolve(OptionCode code) noexcept;

    static bool is_supported(OptionCode code) noexcept;

    static std::vector<OptionCode> get_supported_options() noexcept;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // OS_WINDOWS
#endif // FIZMO_WINSOCK_OPTIONS_HPP