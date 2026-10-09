#include "fizmo_library.hpp"
#include "raw_input_hook.hpp"

namespace fizmo {
namespace input {
namespace detail {

void remove_raw_input_hook(void* context) {
    auto& hooks = raw_input_hooks();
    for (auto it = hooks.begin(); it != hooks.end();) {
        if (it->context == context) it = hooks.erase(it);
        else ++it;
    }
}

bool run_raw_input_hooks(unsigned int message, std::uintptr_t wparam, std::intptr_t lparam) {
    bool handled = false;
    const auto hooks = raw_input_hooks();
    for (const RawInputHookEntry& e : hooks) if (e.hook && e.hook(e.context, message, wparam, lparam)) handled = true;
    return handled;
}

} // namespace detail
} // namespace input
} // namespace fizmo
