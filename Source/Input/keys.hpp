#ifndef FIZMO_INPUT_KEYS_HPP
#define FIZMO_INPUT_KEYS_HPP

#include "../Basic/fizmo_defines.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace fizmo {
namespace input {

enum class Key : std::uint16_t {
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
    Escape, Enter, Tab, Backspace, Space,
    Minus, Equals, LeftBracket, RightBracket, Backslash, Semicolon, Quote, Grave, Comma, Period, Slash, NonUSBackslash,
    CapsLock, ScrollLock, NumLock, PrintScreen, Pause,
    Insert, Delete, Home, End, PageUp, PageDown,
    Left, Right, Up, Down,
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadDivide, NumpadMultiply, NumpadSubtract, NumpadAdd, NumpadEnter, NumpadDecimal, NumpadEquals,
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt, LeftMeta, RightMeta, Menu,
    VolumeMute, VolumeDown, VolumeUp, MediaPlayPause, MediaStop, MediaNext, MediaPrevious,
    BrowserBack, BrowserForward,
    Count
};

inline constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);

enum class Modifiers : std::uint16_t {
    None     = 0,
    Shift    = 1u << 0,
    Control  = 1u << 1,
    Alt      = 1u << 2,
    Meta     = 1u << 3,
    CapsLock = 1u << 4,
    NumLock  = 1u << 5,
    AltGr    = 1u << 6
};

constexpr Modifiers operator|(Modifiers a, Modifiers b) noexcept { return static_cast<Modifiers>(static_cast<std::uint16_t>(a) | static_cast<std::uint16_t>(b)); }
constexpr Modifiers operator&(Modifiers a, Modifiers b) noexcept { return static_cast<Modifiers>(static_cast<std::uint16_t>(a) & static_cast<std::uint16_t>(b)); }
constexpr Modifiers& operator|=(Modifiers& a, Modifiers b) noexcept { a = a | b; return a; }
constexpr bool has(Modifiers set, Modifiers flag) noexcept { return (static_cast<std::uint16_t>(set) & static_cast<std::uint16_t>(flag)) != 0; }

namespace detail {

struct KeyName {
    const char* name;
    Key         key;
};

inline constexpr const char* kKeyNames[kKeyCount] = {
    "Unknown",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
    "Escape", "Enter", "Tab", "Backspace", "Space",
    "Minus", "Equals", "LeftBracket", "RightBracket", "Backslash", "Semicolon", "Quote", "Tilde", "Comma", "Period", "Slash", "NonUSBackslash",
    "CapsLock", "ScrollLock", "NumLock", "PrintScreen", "Pause",
    "Insert", "Delete", "Home", "End", "PageUp", "PageDown",
    "LeftArrow", "RightArrow", "UpArrow", "DownArrow",
    "Numpad0", "Numpad1", "Numpad2", "Numpad3", "Numpad4", "Numpad5", "Numpad6", "Numpad7", "Numpad8", "Numpad9",
    "NumpadDivide", "NumpadMultiply", "NumpadSubtract", "NumpadAdd", "NumpadEnter", "NumpadDecimal", "NumpadEquals",
    "LeftShift", "RightShift", "LeftControl", "RightControl", "LeftAlt", "RightAlt", "LeftMeta", "RightMeta", "Menu",
    "VolumeMute", "VolumeDown", "VolumeUp", "MediaPlayPause", "MediaStop", "MediaNext", "MediaPrevious",
    "BrowserBack", "BrowserForward"
};

inline constexpr KeyName kKeyAliases[] = {
    { "Return", Key::Enter }, { "Esc", Key::Escape }, { "Spacebar", Key::Space }, { " ", Key::Space },
    { "Left", Key::Left }, { "Right", Key::Right }, { "Up", Key::Up }, { "Down", Key::Down },
    { "Shift", Key::LeftShift }, { "Control", Key::LeftControl }, { "Ctrl", Key::LeftControl }, { "Alt", Key::LeftAlt },
    { "LeftCtrl", Key::LeftControl }, { "RightCtrl", Key::RightControl }, { "Super", Key::LeftMeta }, { "Windows", Key::LeftMeta },
    { "Command", Key::LeftMeta }, { "Cmd", Key::LeftMeta }, { "LeftSuper", Key::LeftMeta }, { "RightSuper", Key::RightMeta },
    { "Grave", Key::Grave }, { "Backquote", Key::Grave }, { "Apostrophe", Key::Quote }, { "Del", Key::Delete },
    { "Ins", Key::Insert }, { "PgUp", Key::PageUp }, { "PgDn", Key::PageDown }, { "Caps", Key::CapsLock },
    { "-", Key::Minus }, { "_", Key::Minus }, { "=", Key::Equals }, { "+", Key::Equals }, { "[", Key::LeftBracket }, { "{", Key::LeftBracket },
    { "]", Key::RightBracket }, { "}", Key::RightBracket }, { "\\", Key::Backslash }, { "|", Key::Backslash }, { ";", Key::Semicolon },
    { ":", Key::Semicolon }, { "'", Key::Quote }, { "\"", Key::Quote }, { "`", Key::Grave }, { "~", Key::Grave }, { ",", Key::Comma },
    { "<", Key::Comma }, { ".", Key::Period }, { ">", Key::Period }, { "/", Key::Slash }, { "?", Key::Slash },
    { "!", Key::Num1 }, { "@", Key::Num2 }, { "#", Key::Num3 }, { "$", Key::Num4 }, { "%", Key::Num5 }, { "^", Key::Num6 },
    { "&", Key::Num7 }, { "*", Key::Num8 }, { "(", Key::Num9 }, { ")", Key::Num0 },
    { "Num0", Key::Num0 }, { "Num1", Key::Num1 }, { "Num2", Key::Num2 }, { "Num3", Key::Num3 }, { "Num4", Key::Num4 },
    { "Num5", Key::Num5 }, { "Num6", Key::Num6 }, { "Num7", Key::Num7 }, { "Num8", Key::Num8 }, { "Num9", Key::Num9 },
    { "NumpadSeparator", Key::NumpadDecimal }, { "Apps", Key::Menu }, { "Print", Key::PrintScreen }, { "Break", Key::Pause }
};

constexpr char fold(char c) noexcept { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

constexpr bool same_name(const char* a, const char* b, std::size_t len) noexcept {
    for (std::size_t i = 0; i < len; ++i) { if (b[i] == '\0' || fold(a[i]) != fold(b[i])) return false; }
    return b[len] == '\0';
}

} // namespace detail

constexpr const char* key_name(Key k) noexcept {
    const std::size_t i = static_cast<std::size_t>(k);
    return i < kKeyCount ? detail::kKeyNames[i] : "Unknown";
}

constexpr Key key_from_name(const char* name, std::size_t len) noexcept {
    if (!name || len == 0) return Key::Unknown;
    for (std::size_t i = 1; i < kKeyCount; ++i) if (detail::same_name(name, detail::kKeyNames[i], len)) return static_cast<Key>(i);
    for (const detail::KeyName& a : detail::kKeyAliases) if (detail::same_name(name, a.name, len)) return a.key;
    return Key::Unknown;
}

inline Key key_from_name(const std::string& name) noexcept { return key_from_name(name.data(), name.size()); }
inline Key key_from_name(const char* name) noexcept { return name ? key_from_name(name, std::strlen(name)) : Key::Unknown; }

constexpr bool is_modifier(Key k) noexcept { return k >= Key::LeftShift && k <= Key::RightMeta; }
constexpr bool is_letter(Key k) noexcept { return k >= Key::A && k <= Key::Z; }
constexpr bool is_digit(Key k) noexcept { return k >= Key::Num0 && k <= Key::Num9; }
constexpr bool is_function(Key k) noexcept { return k >= Key::F1 && k <= Key::F24; }
constexpr bool is_numpad(Key k) noexcept { return k >= Key::Numpad0 && k <= Key::NumpadEquals; }

constexpr Key key_from_char(char32_t c) noexcept {
    if (c >= 'a' && c <= 'z') return static_cast<Key>(static_cast<std::uint16_t>(Key::A) + (c - 'a'));
    if (c >= 'A' && c <= 'Z') return static_cast<Key>(static_cast<std::uint16_t>(Key::A) + (c - 'A'));
    if (c >= '0' && c <= '9') return static_cast<Key>(static_cast<std::uint16_t>(Key::Num0) + (c - '0'));

    switch (c) {
        case ' ':  return Key::Space;
        case '-': case '_': return Key::Minus;
        case '=': case '+': return Key::Equals;
        case '[': case '{': return Key::LeftBracket;
        case ']': case '}': return Key::RightBracket;
        case '\\': case '|': return Key::Backslash;
        case ';': case ':': return Key::Semicolon;
        case '\'': case '"': return Key::Quote;
        case '`': case '~': return Key::Grave;
        case ',': case '<': return Key::Comma;
        case '.': case '>': return Key::Period;
        case '/': case '?': return Key::Slash;
        case '\t': return Key::Tab;
        case '\r': case '\n': return Key::Enter;
        case 8:    return Key::Backspace;
        case 27:   return Key::Escape;
        default:   return Key::Unknown;
    }
}

namespace detail {

inline constexpr Key kEvdevKeys[] = {
    Key::Unknown, Key::Escape, Key::Num1, Key::Num2, Key::Num3, Key::Num4, Key::Num5, Key::Num6, Key::Num7, Key::Num8,
    Key::Num9, Key::Num0, Key::Minus, Key::Equals, Key::Backspace, Key::Tab, Key::Q, Key::W, Key::E, Key::R,
    Key::T, Key::Y, Key::U, Key::I, Key::O, Key::P, Key::LeftBracket, Key::RightBracket, Key::Enter, Key::LeftControl,
    Key::A, Key::S, Key::D, Key::F, Key::G, Key::H, Key::J, Key::K, Key::L, Key::Semicolon,
    Key::Quote, Key::Grave, Key::LeftShift, Key::Backslash, Key::Z, Key::X, Key::C, Key::V, Key::B, Key::N,
    Key::M, Key::Comma, Key::Period, Key::Slash, Key::RightShift, Key::NumpadMultiply, Key::LeftAlt, Key::Space, Key::CapsLock, Key::F1,
    Key::F2, Key::F3, Key::F4, Key::F5, Key::F6, Key::F7, Key::F8, Key::F9, Key::F10, Key::NumLock,
    Key::ScrollLock, Key::Numpad7, Key::Numpad8, Key::Numpad9, Key::NumpadSubtract, Key::Numpad4, Key::Numpad5, Key::Numpad6, Key::NumpadAdd, Key::Numpad1,
    Key::Numpad2, Key::Numpad3, Key::Numpad0, Key::NumpadDecimal, Key::Unknown, Key::Unknown, Key::NonUSBackslash, Key::F11, Key::F12, Key::Unknown,
    Key::Unknown, Key::Unknown, Key::Unknown, Key::Unknown, Key::Unknown, Key::Unknown, Key::NumpadEnter, Key::RightControl, Key::NumpadDivide, Key::PrintScreen,
    Key::RightAlt, Key::Unknown, Key::Home, Key::Up, Key::PageUp, Key::Left, Key::Right, Key::End, Key::Down, Key::PageDown,
    Key::Insert, Key::Delete, Key::Unknown, Key::VolumeMute, Key::VolumeDown, Key::VolumeUp, Key::Unknown, Key::NumpadEquals, Key::Unknown, Key::Pause,
    Key::Unknown, Key::Unknown, Key::Unknown, Key::Unknown, Key::Unknown, Key::LeftMeta, Key::RightMeta, Key::Menu
};

} // namespace detail

constexpr Key key_from_evdev(unsigned int code) noexcept {
    if (code < sizeof(detail::kEvdevKeys) / sizeof(detail::kEvdevKeys[0])) return detail::kEvdevKeys[code];

    switch (code) {
        case 158: return Key::BrowserBack;
        case 159: return Key::BrowserForward;
        case 163: return Key::MediaNext;
        case 164: return Key::MediaPlayPause;
        case 165: return Key::MediaPrevious;
        case 166: return Key::MediaStop;
        default: break;
    }

    if (code >= 183 && code <= 194) return static_cast<Key>(static_cast<std::uint16_t>(Key::F13) + (code - 183));
    return Key::Unknown;
}

constexpr Key key_from_x11_keycode(unsigned int keycode) noexcept { return keycode >= 8 ? key_from_evdev(keycode - 8) : Key::Unknown; }

constexpr Key key_from_set1_scancode(unsigned int code, bool extended) noexcept {
    if (!extended) {
        switch (code) {
            case 0x47: return Key::Numpad7;
            case 0x48: return Key::Numpad8;
            case 0x49: return Key::Numpad9;
            case 0x4B: return Key::Numpad4;
            case 0x4C: return Key::Numpad5;
            case 0x4D: return Key::Numpad6;
            case 0x4F: return Key::Numpad1;
            case 0x50: return Key::Numpad2;
            case 0x51: return Key::Numpad3;
            case 0x52: return Key::Numpad0;
            case 0x53: return Key::NumpadDecimal;
            case 0x54: return Key::PrintScreen;
            case 0x56: return Key::NonUSBackslash;
            case 0x57: return Key::F11;
            case 0x58: return Key::F12;
            case 0x59: return Key::NumpadEquals;
            case 0x45: return Key::Pause;
            default: break;
        }
        if (code >= 0x64 && code <= 0x6E) return static_cast<Key>(static_cast<std::uint16_t>(Key::F13) + (code - 0x64));
        if (code == 0x76) return Key::F24;
        return code < 0x47 ? key_from_evdev(code) : Key::Unknown;
    }

    switch (code) {
        case 0x1C: return Key::NumpadEnter;
        case 0x1D: return Key::RightControl;
        case 0x35: return Key::NumpadDivide;
        case 0x37: return Key::PrintScreen;
        case 0x38: return Key::RightAlt;
        case 0x45: return Key::NumLock;
        case 0x46: return Key::Pause;
        case 0x47: return Key::Home;
        case 0x48: return Key::Up;
        case 0x49: return Key::PageUp;
        case 0x4B: return Key::Left;
        case 0x4D: return Key::Right;
        case 0x4F: return Key::End;
        case 0x50: return Key::Down;
        case 0x51: return Key::PageDown;
        case 0x52: return Key::Insert;
        case 0x53: return Key::Delete;
        case 0x5B: return Key::LeftMeta;
        case 0x5C: return Key::RightMeta;
        case 0x5D: return Key::Menu;
        case 0x20: return Key::VolumeMute;
        case 0x2E: return Key::VolumeDown;
        case 0x30: return Key::VolumeUp;
        case 0x22: return Key::MediaPlayPause;
        case 0x24: return Key::MediaStop;
        case 0x19: return Key::MediaNext;
        case 0x10: return Key::MediaPrevious;
        case 0x6A: return Key::BrowserBack;
        case 0x69: return Key::BrowserForward;
        default:   return Key::Unknown;
    }
}

constexpr Key key_from_windows_vk(unsigned int vk) noexcept {
    if (vk >= 'A' && vk <= 'Z') return static_cast<Key>(static_cast<std::uint16_t>(Key::A) + (vk - 'A'));
    if (vk >= '0' && vk <= '9') return static_cast<Key>(static_cast<std::uint16_t>(Key::Num0) + (vk - '0'));
    if (vk >= 0x70 && vk <= 0x87) return static_cast<Key>(static_cast<std::uint16_t>(Key::F1) + (vk - 0x70));
    if (vk >= 0x60 && vk <= 0x69) return static_cast<Key>(static_cast<std::uint16_t>(Key::Numpad0) + (vk - 0x60));

    switch (vk) {
        case 0x08: return Key::Backspace;
        case 0x09: return Key::Tab;
        case 0x0D: return Key::Enter;
        case 0x13: return Key::Pause;
        case 0x14: return Key::CapsLock;
        case 0x1B: return Key::Escape;
        case 0x20: return Key::Space;
        case 0x21: return Key::PageUp;
        case 0x22: return Key::PageDown;
        case 0x23: return Key::End;
        case 0x24: return Key::Home;
        case 0x25: return Key::Left;
        case 0x26: return Key::Up;
        case 0x27: return Key::Right;
        case 0x28: return Key::Down;
        case 0x2C: return Key::PrintScreen;
        case 0x2D: return Key::Insert;
        case 0x2E: return Key::Delete;
        case 0x5B: return Key::LeftMeta;
        case 0x5C: return Key::RightMeta;
        case 0x5D: return Key::Menu;
        case 0x6A: return Key::NumpadMultiply;
        case 0x6B: return Key::NumpadAdd;
        case 0x6D: return Key::NumpadSubtract;
        case 0x6E: return Key::NumpadDecimal;
        case 0x6F: return Key::NumpadDivide;
        case 0x90: return Key::NumLock;
        case 0x91: return Key::ScrollLock;
        case 0xA0: return Key::LeftShift;
        case 0xA1: return Key::RightShift;
        case 0xA2: return Key::LeftControl;
        case 0xA3: return Key::RightControl;
        case 0xA4: return Key::LeftAlt;
        case 0xA5: return Key::RightAlt;
        case 0xA6: return Key::BrowserBack;
        case 0xA7: return Key::BrowserForward;
        case 0xAD: return Key::VolumeMute;
        case 0xAE: return Key::VolumeDown;
        case 0xAF: return Key::VolumeUp;
        case 0xB0: return Key::MediaNext;
        case 0xB1: return Key::MediaPrevious;
        case 0xB2: return Key::MediaStop;
        case 0xB3: return Key::MediaPlayPause;
        case 0xBA: return Key::Semicolon;
        case 0xBB: return Key::Equals;
        case 0xBC: return Key::Comma;
        case 0xBD: return Key::Minus;
        case 0xBE: return Key::Period;
        case 0xBF: return Key::Slash;
        case 0xC0: return Key::Grave;
        case 0xDB: return Key::LeftBracket;
        case 0xDC: return Key::Backslash;
        case 0xDD: return Key::RightBracket;
        case 0xDE: return Key::Quote;
        case 0xE2: return Key::NonUSBackslash;
        default:   return Key::Unknown;
    }
}

constexpr Key key_from_x11_keysym(unsigned long ks) noexcept {
    if (ks >= 0x61 && ks <= 0x7A) return static_cast<Key>(static_cast<std::uint16_t>(Key::A) + (ks - 0x61));
    if (ks >= 0x41 && ks <= 0x5A) return static_cast<Key>(static_cast<std::uint16_t>(Key::A) + (ks - 0x41));
    if (ks >= 0x30 && ks <= 0x39) return static_cast<Key>(static_cast<std::uint16_t>(Key::Num0) + (ks - 0x30));
    if (ks >= 0xFFBE && ks <= 0xFFD5) return static_cast<Key>(static_cast<std::uint16_t>(Key::F1) + (ks - 0xFFBE));
    if (ks >= 0xFFB0 && ks <= 0xFFB9) return static_cast<Key>(static_cast<std::uint16_t>(Key::Numpad0) + (ks - 0xFFB0));
    if (ks < 0x80) return key_from_char(static_cast<char32_t>(ks));

    switch (ks) {
        case 0xFF08: return Key::Backspace;
        case 0xFF09: case 0xFE20: return Key::Tab;
        case 0xFF0D: return Key::Enter;
        case 0xFF13: return Key::Pause;
        case 0xFF14: return Key::ScrollLock;
        case 0xFF1B: return Key::Escape;
        case 0xFF50: return Key::Home;
        case 0xFF51: return Key::Left;
        case 0xFF52: return Key::Up;
        case 0xFF53: return Key::Right;
        case 0xFF54: return Key::Down;
        case 0xFF55: return Key::PageUp;
        case 0xFF56: return Key::PageDown;
        case 0xFF57: return Key::End;
        case 0xFF61: return Key::PrintScreen;
        case 0xFF63: return Key::Insert;
        case 0xFF67: return Key::Menu;
        case 0xFF7F: return Key::NumLock;
        case 0xFF8D: return Key::NumpadEnter;
        case 0xFF95: return Key::Numpad7;
        case 0xFF96: return Key::Numpad4;
        case 0xFF97: return Key::Numpad8;
        case 0xFF98: return Key::Numpad6;
        case 0xFF99: return Key::Numpad2;
        case 0xFF9A: return Key::Numpad9;
        case 0xFF9B: return Key::Numpad3;
        case 0xFF9C: return Key::Numpad1;
        case 0xFF9D: return Key::Numpad5;
        case 0xFF9E: return Key::Numpad0;
        case 0xFF9F: return Key::NumpadDecimal;
        case 0xFFAA: return Key::NumpadMultiply;
        case 0xFFAB: return Key::NumpadAdd;
        case 0xFFAC: return Key::NumpadDecimal;
        case 0xFFAD: return Key::NumpadSubtract;
        case 0xFFAE: return Key::NumpadDecimal;
        case 0xFFAF: return Key::NumpadDivide;
        case 0xFFBD: return Key::NumpadEquals;
        case 0xFFE1: return Key::LeftShift;
        case 0xFFE2: return Key::RightShift;
        case 0xFFE3: return Key::LeftControl;
        case 0xFFE4: return Key::RightControl;
        case 0xFFE5: return Key::CapsLock;
        case 0xFFE9: return Key::LeftAlt;
        case 0xFFEA: case 0xFE03: return Key::RightAlt;
        case 0xFFEB: case 0xFFE7: return Key::LeftMeta;
        case 0xFFEC: case 0xFFE8: return Key::RightMeta;
        case 0xFFFF: return Key::Delete;
        case 0x1008FF12: return Key::VolumeMute;
        case 0x1008FF11: return Key::VolumeDown;
        case 0x1008FF13: return Key::VolumeUp;
        case 0x1008FF14: return Key::MediaPlayPause;
        case 0x1008FF15: return Key::MediaStop;
        case 0x1008FF16: return Key::MediaPrevious;
        case 0x1008FF17: return Key::MediaNext;
        case 0x1008FF26: return Key::BrowserBack;
        case 0x1008FF27: return Key::BrowserForward;
        default: return Key::Unknown;
    }
}

} // namespace input
} // namespace fizmo

#endif // FIZMO_INPUT_KEYS_HPP
