#ifndef FIZMO_INPUT_RAW_INPUT_HOOK_HPP
#define FIZMO_INPUT_RAW_INPUT_HOOK_HPP

#include <cstdint>
#include <mutex>
#include <vector>

namespace fizmo {
namespace input {
namespace detail {

using RawInputHook = bool (*)(void* context, unsigned int message, std::uintptr_t wparam, std::intptr_t lparam);

struct RawInputHookEntry {
    RawInputHook hook    = nullptr;
    void*        context = nullptr;
};

inline std::vector<RawInputHookEntry>& raw_input_hooks() {
    static std::vector<RawInputHookEntry> hooks;
    return hooks;
}

inline void add_raw_input_hook(RawInputHook hook, void* context) {
    raw_input_hooks().push_back({ hook, context });
}

inline void remove_raw_input_hook(void* context) {
    auto& hooks = raw_input_hooks();
    for (auto it = hooks.begin(); it != hooks.end();) {
        if (it->context == context) it = hooks.erase(it);
        else ++it;
    }
}

inline bool run_raw_input_hooks(unsigned int message, std::uintptr_t wparam, std::intptr_t lparam) {
    bool handled = false;
    const auto hooks = raw_input_hooks();
    for (const RawInputHookEntry& e : hooks) if (e.hook && e.hook(e.context, message, wparam, lparam)) handled = true;
    return handled;
}

} // namespace detail
} // namespace input
} // namespace fizmo

#endif // FIZMO_INPUT_RAW_INPUT_HOOK_HPP
