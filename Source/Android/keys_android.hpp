#ifndef FIZMO_ANDROID_KEYS_HPP
#define FIZMO_ANDROID_KEYS_HPP

#include "android_types.hpp"

#if defined(OS_ANDROID)

namespace fizmo {
namespace android {
namespace detail {

input::Key key_from_android(int key_code) noexcept;
input::Modifiers mods_from_android(int meta_state) noexcept;
bool gamepad_button_from_android(int key_code, input::GamepadButton& out) noexcept;
bool is_system_key(int key_code) noexcept;

} // namespace detail
} // namespace android
} // namespace fizmo

#endif // OS_ANDROID
#endif // FIZMO_ANDROID_KEYS_HPP
