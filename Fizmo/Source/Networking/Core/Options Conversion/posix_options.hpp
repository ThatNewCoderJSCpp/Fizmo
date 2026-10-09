#ifndef FIZMO_POSIX_OPTIONS_HPP
#define FIZMO_POSIX_OPTIONS_HPP

#include "socket_option.hpp"

#ifdef OS_LINUX

namespace fizmo {
namespace networking {
namespace core {

class LinuxOptions {
public:
    static NativeOption resolve(OptionCode code) noexcept;

    static bool is_supported(OptionCode code) noexcept { return resolve(code).supported; }

    static std::vector<OptionCode> get_supported_options() noexcept;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // OS_LINUX
#endif // FIZMO_POSIX_OPTIONS_HPP