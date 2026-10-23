#include "keys_android.hpp"

#if defined(OS_ANDROID)

#include <android/input.h>
#include <android/keycodes.h>

namespace fizmo {
namespace android {
namespace detail {

input::Key key_from_android(int code) noexcept {
    using input::Key;
    if (code >= AKEYCODE_A && code <= AKEYCODE_Z) return static_cast<Key>(static_cast<int>(Key::A) + (code - AKEYCODE_A));
    if (code >= AKEYCODE_0 && code <= AKEYCODE_9) return static_cast<Key>(static_cast<int>(Key::Num0) + (code - AKEYCODE_0));
    if (code >= AKEYCODE_F1 && code <= AKEYCODE_F12) return static_cast<Key>(static_cast<int>(Key::F1) + (code - AKEYCODE_F1));
    if (code >= AKEYCODE_NUMPAD_0 && code <= AKEYCODE_NUMPAD_9) return static_cast<Key>(static_cast<int>(Key::Numpad0) + (code - AKEYCODE_NUMPAD_0));
    switch (code) {
        case AKEYCODE_ESCAPE:          return Key::Escape;
        case AKEYCODE_ENTER:           return Key::Enter;
        case AKEYCODE_TAB:             return Key::Tab;
        case AKEYCODE_DEL:             return Key::Backspace;
        case AKEYCODE_FORWARD_DEL:     return Key::Delete;
        case AKEYCODE_SPACE:           return Key::Space;
        case AKEYCODE_MINUS:           return Key::Minus;
        case AKEYCODE_EQUALS:          return Key::Equals;
        case AKEYCODE_LEFT_BRACKET:    return Key::LeftBracket;
        case AKEYCODE_RIGHT_BRACKET:   return Key::RightBracket;
        case AKEYCODE_BACKSLASH:       return Key::Backslash;
        case AKEYCODE_SEMICOLON:       return Key::Semicolon;
        case AKEYCODE_APOSTROPHE:      return Key::Quote;
        case AKEYCODE_GRAVE:           return Key::Grave;
        case AKEYCODE_COMMA:           return Key::Comma;
        case AKEYCODE_PERIOD:          return Key::Period;
        case AKEYCODE_SLASH:           return Key::Slash;
        case AKEYCODE_CAPS_LOCK:       return Key::CapsLock;
        case AKEYCODE_SCROLL_LOCK:     return Key::ScrollLock;
        case AKEYCODE_NUM_LOCK:        return Key::NumLock;
        case AKEYCODE_SYSRQ:           return Key::PrintScreen;
        case AKEYCODE_BREAK:           return Key::Pause;
        case AKEYCODE_INSERT:          return Key::Insert;
        case AKEYCODE_MOVE_HOME:       return Key::Home;
        case AKEYCODE_MOVE_END:        return Key::End;
        case AKEYCODE_PAGE_UP:         return Key::PageUp;
        case AKEYCODE_PAGE_DOWN:       return Key::PageDown;
        case AKEYCODE_DPAD_LEFT:       return Key::Left;
        case AKEYCODE_DPAD_RIGHT:      return Key::Right;
        case AKEYCODE_DPAD_UP:         return Key::Up;
        case AKEYCODE_DPAD_DOWN:       return Key::Down;
        case AKEYCODE_DPAD_CENTER:     return Key::Enter;
        case AKEYCODE_NUMPAD_DIVIDE:   return Key::NumpadDivide;
        case AKEYCODE_NUMPAD_MULTIPLY: return Key::NumpadMultiply;
        case AKEYCODE_NUMPAD_SUBTRACT: return Key::NumpadSubtract;
        case AKEYCODE_NUMPAD_ADD:      return Key::NumpadAdd;
        case AKEYCODE_NUMPAD_ENTER:    return Key::NumpadEnter;
        case AKEYCODE_NUMPAD_DOT:      return Key::NumpadDecimal;
        case AKEYCODE_NUMPAD_EQUALS:   return Key::NumpadEquals;
        case AKEYCODE_SHIFT_LEFT:      return Key::LeftShift;
        case AKEYCODE_SHIFT_RIGHT:     return Key::RightShift;
        case AKEYCODE_CTRL_LEFT:       return Key::LeftControl;
        case AKEYCODE_CTRL_RIGHT:      return Key::RightControl;
        case AKEYCODE_ALT_LEFT:        return Key::LeftAlt;
        case AKEYCODE_ALT_RIGHT:       return Key::RightAlt;
        case AKEYCODE_META_LEFT:       return Key::LeftMeta;
        case AKEYCODE_META_RIGHT:      return Key::RightMeta;
        case AKEYCODE_MENU:            return Key::Menu;
        case AKEYCODE_VOLUME_MUTE:     return Key::VolumeMute;
        case AKEYCODE_VOLUME_DOWN:     return Key::VolumeDown;
        case AKEYCODE_VOLUME_UP:       return Key::VolumeUp;
        case AKEYCODE_MEDIA_PLAY_PAUSE: return Key::MediaPlayPause;
        case AKEYCODE_MEDIA_STOP:      return Key::MediaStop;
        case AKEYCODE_MEDIA_NEXT:      return Key::MediaNext;
        case AKEYCODE_MEDIA_PREVIOUS:  return Key::MediaPrevious;
        case AKEYCODE_BACK:            return Key::BrowserBack;
        case AKEYCODE_FORWARD:         return Key::BrowserForward;
        default:                       return Key::Unknown;
    }
}

input::Modifiers mods_from_android(int meta) noexcept {
    using input::Modifiers;
    Modifiers m = Modifiers::None;
    if (meta & AMETA_SHIFT_ON)     m |= Modifiers::Shift;
    if (meta & AMETA_CTRL_ON)      m |= Modifiers::Control;
    if (meta & AMETA_ALT_RIGHT_ON) m |= Modifiers::AltGr;
    if (meta & AMETA_ALT_ON)       m |= Modifiers::Alt;
    if (meta & AMETA_META_ON)      m |= Modifiers::Meta;
    if (meta & AMETA_CAPS_LOCK_ON) m |= Modifiers::CapsLock;
    if (meta & AMETA_NUM_LOCK_ON)  m |= Modifiers::NumLock;
    return m;
}

bool gamepad_button_from_android(int code, input::GamepadButton& out) noexcept {
    using input::GamepadButton;
    switch (code) {
        case AKEYCODE_BUTTON_A:      out = GamepadButton::South; return true;
        case AKEYCODE_BUTTON_B:      out = GamepadButton::East; return true;
        case AKEYCODE_BUTTON_X:      out = GamepadButton::West; return true;
        case AKEYCODE_BUTTON_Y:      out = GamepadButton::North; return true;
        case AKEYCODE_BUTTON_SELECT: out = GamepadButton::Back; return true;
        case AKEYCODE_BUTTON_MODE:   out = GamepadButton::Guide; return true;
        case AKEYCODE_BUTTON_START:  out = GamepadButton::Start; return true;
        case AKEYCODE_BUTTON_THUMBL: out = GamepadButton::LeftStick; return true;
        case AKEYCODE_BUTTON_THUMBR: out = GamepadButton::RightStick; return true;
        case AKEYCODE_BUTTON_L1:     out = GamepadButton::LeftShoulder; return true;
        case AKEYCODE_BUTTON_R1:     out = GamepadButton::RightShoulder; return true;
        case AKEYCODE_DPAD_UP:       out = GamepadButton::DpadUp; return true;
        case AKEYCODE_DPAD_DOWN:     out = GamepadButton::DpadDown; return true;
        case AKEYCODE_DPAD_LEFT:     out = GamepadButton::DpadLeft; return true;
        case AKEYCODE_DPAD_RIGHT:    out = GamepadButton::DpadRight; return true;
        default:                     return false;
    }
}

bool is_system_key(int code) noexcept {
    switch (code) {
        case AKEYCODE_VOLUME_UP:
        case AKEYCODE_VOLUME_DOWN:
        case AKEYCODE_VOLUME_MUTE:
        case AKEYCODE_POWER:
        case AKEYCODE_CAMERA:
        case AKEYCODE_HOME:
        case AKEYCODE_APP_SWITCH:
        case AKEYCODE_ASSIST:
        case AKEYCODE_VOICE_ASSIST:
        case AKEYCODE_SEARCH:
        case AKEYCODE_MEDIA_PLAY_PAUSE:
        case AKEYCODE_MEDIA_PLAY:
        case AKEYCODE_MEDIA_PAUSE:
        case AKEYCODE_MEDIA_NEXT:
        case AKEYCODE_MEDIA_PREVIOUS:
        case AKEYCODE_MEDIA_STOP:
        case AKEYCODE_HEADSETHOOK:
            return true;
        default:
            return false;
    }
}

} // namespace detail
} // namespace android
} // namespace fizmo

#endif // OS_ANDROID
