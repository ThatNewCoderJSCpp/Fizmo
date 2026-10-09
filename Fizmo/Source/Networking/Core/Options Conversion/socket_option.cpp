#include "fizmo_library.hpp"
#include "socket_option.hpp"

namespace fizmo {
namespace networking {
namespace core {

const char* SocketOption::code_to_string(OptionCode code) noexcept {
    switch (code) {
    #define FIZMO_TO_STRING(name, cat, type) case OptionCode::name: return #name;
        FIZMO_ALL_OPTIONS(FIZMO_TO_STRING)
    #undef FIZMO_TO_STRING
        default: return "UnknownOption";
    }
}

auto SocketOption::get_category_for_code(OptionCode code) noexcept -> OptionCategory {
    switch (code) {
    #define FIZMO_CATEGORY(name, cat, type) case OptionCode::name: return OptionCategory::cat;
        FIZMO_ALL_OPTIONS(FIZMO_CATEGORY)
    #undef FIZMO_CATEGORY
        default: return OptionCategory::Common;
    }
}

auto SocketOption::get_type_for_code(OptionCode code) noexcept -> OptionType {
    switch (code) {
    #define FIZMO_TYPE(name, cat, type) case OptionCode::name: return OptionType::type;
        FIZMO_ALL_OPTIONS(FIZMO_TYPE)
    #undef FIZMO_TYPE
        default: return OptionType::Boolean;
    }
}

bool SocketOption::is_available_on_current_platform(OptionCode code) noexcept {
    OptionCategory cat = get_category_for_code(code);
    if (cat == OptionCategory::Common) return true;
#ifdef OS_WINDOWS
    return cat == OptionCategory::Windows;
#elif defined(OS_LINUX)
    return cat == OptionCategory::Linux;
#else
    return false;
#endif
}

const char* SocketOption::category_to_string(OptionCategory category) noexcept {
    switch (category) {
        case OptionCategory::Common:  return "Common";
        case OptionCategory::Windows: return "Windows";
        case OptionCategory::Linux:   return "Linux";
        default:                      return "Unknown";
    }
}

} // namespace core
} // namespace networking
} // namespace fizmo
