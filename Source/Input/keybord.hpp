#ifndef FIZMO_KEYBOARD_INPUT_HPP
#define FIZMO_KEYBOARD_INPUT_HPP

#include "../Basic/fizmo_defines.hpp"

namespace fizmo {
namespace input {

struct KeyInfo {
    const char* name;
    unsigned int code;      // Virtual key code 
    unsigned int scancode;  // Hardware scancode (includes extended flag)
};

class Keyboard {
private:
    static constexpr KeyInfo key_table[] = {
        // Row 1 - Function keys
        {"Escape", 0x1B, 0x01},
        {"F1", 0x70, 0x3B},
        {"F2", 0x71, 0x3C},
        {"F3", 0x72, 0x3D},
        {"F4", 0x73, 0x3E},
        {"F5", 0x74, 0x3F},
        {"F6", 0x75, 0x40},
        {"F7", 0x76, 0x41},
        {"F8", 0x77, 0x42},
        {"F9", 0x78, 0x43},
        {"F10", 0x79, 0x44},
        {"F11", 0x7A, 0x57},
        {"F12", 0x7B, 0x58},

        // Row 2 - Number row
        {"Tilde", 0xC0, 0x29},
        {"1", '1', 0x02},
        {"2", '2', 0x03},
        {"3", '3', 0x04},
        {"4", '4', 0x05},
        {"5", '5', 0x06},
        {"6", '6', 0x07},
        {"7", '7', 0x08},
        {"8", '8', 0x09},
        {"9", '9', 0x0A},
        {"0", '0', 0x0B},
        {"Minus", 0xBD, 0x0C},
        {"Equals", 0xBB, 0x0D},
        {"Backspace", 0x08, 0x0E},

        // Row 3
        {"Tab", 0x09, 0x0F},
        {"Q", 'Q', 0x10},
        {"W", 'W', 0x11},
        {"E", 'E', 0x12},
        {"R", 'R', 0x13},
        {"T", 'T', 0x14},
        {"Y", 'Y', 0x15},
        {"U", 'U', 0x16},
        {"I", 'I', 0x17},
        {"O", 'O', 0x18},
        {"P", 'P', 0x19},
        {"LeftBracket", 0xDB, 0x1A},
        {"RightBracket", 0xDD, 0x1B},
        {"Backslash", 0xDC, 0x2B},

        // Row 4
        {"CapsLock", 0x14, 0x3A},
        {"A", 'A', 0x1E},
        {"S", 'S', 0x1F},
        {"D", 'D', 0x20},
        {"F", 'F', 0x21},
        {"G", 'G', 0x22},
        {"H", 'H', 0x23},
        {"J", 'J', 0x24},
        {"K", 'K', 0x25},
        {"L", 'L', 0x26},
        {"Semicolon", 0xBA, 0x27},
        {"Quote", 0xDE, 0x28},
        {"Enter", 0x0D, 0x1C},

        // Row 5
        {"LeftShift", 0x10, 0x2A},
        {"Z", 'Z', 0x2C},
        {"X", 'X', 0x2D},
        {"C", 'C', 0x2E},
        {"V", 'V', 0x2F},
        {"B", 'B', 0x30},
        {"N", 'N', 0x31},
        {"M", 'M', 0x32},
        {"Comma", 0xBC, 0x33},
        {"Period", 0xBE, 0x34},
        {"Slash", 0xBF, 0x35},
        {"RightShift", 0x10, 0x36},

        // Row 6
        {"LeftControl", 0x11, 0x1D},
        {"LeftAlt", 0x12, 0x38},
        {"Space", 0x20, 0x39},

        {"PrintScreen", 0x2C, 0xE037},
        {"ScrollLock", 0x91, 0x46},
        {"Pause", 0x13, 0x45},         

        // Extended keys (0xE000 | scancode)
        {"UpArrow", 0x26, 0xE048},
        {"DownArrow", 0x28, 0xE050},
        {"LeftArrow", 0x25, 0xE04B},
        {"RightArrow", 0x27, 0xE04D},
        {"Home", 0x24, 0xE047},
        {"End", 0x23, 0xE04F},
        {"PageUp", 0x21, 0xE049},
        {"PageDown", 0x22, 0xE051},
        {"Insert", 0x2D, 0xE052},
        {"Delete", 0x2E, 0xE053},
        {"RightControl", 0x11, 0xE01D},
        {"RightAlt", 0x12, 0xE038},
        {"LeftMeta", 0x5B, 0xE05B},      // Windows key, Command, Super
        {"RightMeta", 0x5C, 0xE05C},     // Windows key, Command, Super
        {"Menu", 0x5D, 0xE05D},

        // Numpad 
        {"Numpad0", 0x60, 0x52},
        {"Numpad1", 0x61, 0x4F},
        {"Numpad2", 0x62, 0x50},
        {"Numpad3", 0x63, 0x51},
        {"Numpad4", 0x64, 0x4B},
        {"Numpad5", 0x65, 0x4C},
        {"Numpad6", 0x66, 0x4D},
        {"Numpad7", 0x67, 0x47},
        {"Numpad8", 0x68, 0x48},
        {"Numpad9", 0x69, 0x49},

        // Numpad - Operators
        {"NumpadMultiply", 0x6A, 0x37},
        {"NumpadAdd", 0x6B, 0x4E},
        {"NumpadSubtract", 0x6D, 0x4A},
        {"NumpadDecimal", 0x6E, 0x53},
        {"NumpadDivide", 0x6F, 0xE035},  
        {"NumpadEnter", 0x0D, 0xE01C},    
        {"NumpadSeparator", 0x6C, 0x53},
        {"NumLock", 0x90, 0x45},
    };

    static constexpr unsigned int table_size = sizeof(key_table) / sizeof(KeyInfo);

private:
    static constexpr char to_uppercase(char c) noexcept { return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c; }

    static constexpr bool str_equal_ignore_case(const char* a, const char* b) noexcept {
        if (a == nullptr || b == nullptr) return false;
        
        while (*a && *b) {
            if (to_uppercase(*a) != to_uppercase(*b)) { return false; }
            ++a;
            ++b;
        }

        return *a == *b; 
    }

public:
    static constexpr KeyInfo get_key_info(unsigned int scancode, bool extended) noexcept {
        if (scancode == 0) { return {"Unknown", 0, 0}; }
        unsigned int lookup_code = extended ? (0xE000u | scancode) : scancode;
        for (unsigned int i = 0; i < table_size; ++i) { if (key_table[i].scancode == lookup_code) { return key_table[i]; }}
        return {"Unknown", 0, scancode};
    }

    static constexpr const char* get_shifted_symbol(unsigned int scancode) noexcept {
        switch(scancode) {
            case 0x29: return "~";
            case 0x02: return "!";
            case 0x03: return "@";
            case 0x04: return "#";
            case 0x05: return "$";
            case 0x06: return "%";
            case 0x07: return "^";
            case 0x08: return "&";
            case 0x09: return "*";
            case 0x0A: return "(";
            case 0x0B: return ")";
            case 0x0C: return "_";
            case 0x0D: return "+";
            case 0x1A: return "{";
            case 0x1B: return "}";
            case 0x2B: return "|";
            case 0x27: return ":";
            case 0x28: return "\"";
            case 0x33: return "<";
            case 0x34: return ">";
            case 0x35: return "?";
            default: return "";
        }
    }

    static constexpr KeyInfo get_key_by_code(unsigned int code) noexcept {
        for (unsigned int i = 0; i < table_size; ++i) { if (key_table[i].code == code) { return key_table[i]; }}
        return {"Unknown", 0, 0};
    }

    static constexpr KeyInfo get_key_by_scancode(unsigned int scancode) noexcept {
        for (unsigned int i = 0; i < table_size; ++i) { if (key_table[i].scancode == scancode) { return key_table[i]; }}
        return {"Unknown", 0, 0};
    }

    static constexpr unsigned int get_code_by_name(const char* name) noexcept {
        if (name == nullptr) return 0;
        for (unsigned int i = 0; i < table_size; ++i) { if (str_equal_ignore_case(key_table[i].name, name)) { return key_table[i].code; }}
        return 0;
    }

    static constexpr unsigned int get_scancode_by_name(const char* name) noexcept {
        if (name == nullptr) return 0;
        for (unsigned int i = 0; i < table_size; ++i) { if (str_equal_ignore_case(key_table[i].name, name)) { return key_table[i].scancode; }}
        return 0;
    }
};

constexpr KeyInfo Keyboard::key_table[];

#ifdef OS_WINDOWS

#endif // OS_WINDOWS

} // namespace input
} // namespace fizmo

#endif // FIZMO_KEYBOARD_INPUT_HPP