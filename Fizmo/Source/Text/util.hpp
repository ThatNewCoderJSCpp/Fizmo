#ifndef FIZMO_TEXT_UTIL_HPP
#define FIZMO_TEXT_UTIL_HPP

#include "fonts.hpp"

namespace fizmo {
namespace text {

enum class FontWeight : std::uint16_t {
    Thin       = 100,
    ExtraLight = 200,
    Light      = 300,
    Normal     = 400,
    Medium     = 500,
    SemiBold   = 600,
    Bold       = 700,
    ExtraBold  = 800,
    Black      = 900
};

constexpr bool is_bold(FontWeight w) noexcept { return static_cast<std::uint16_t>(w) >= 700; }

inline std::ostream& operator<<(std::ostream& os, FontWeight w) {
    switch (w) {
        case FontWeight::Thin:       return os << "Thin(100)";
        case FontWeight::ExtraLight: return os << "ExtraLight(200)";
        case FontWeight::Light:      return os << "Light(300)";
        case FontWeight::Normal:     return os << "Normal(400)";
        case FontWeight::Medium:     return os << "Medium(500)";
        case FontWeight::SemiBold:   return os << "SemiBold(600)";
        case FontWeight::Bold:       return os << "Bold(700)";
        case FontWeight::ExtraBold:  return os << "ExtraBold(800)";
        case FontWeight::Black:      return os << "Black(900)";
        default:                     return os << "Weight(" << static_cast<int>(w) << ")";
    }
}

enum class FontSlant : std::uint8_t {
    Normal  = 0,
    Italic  = 1,
    Oblique = 2
};

inline std::ostream& operator<<(std::ostream& os, FontSlant s) {
    switch (s) {
        case FontSlant::Normal:  return os << "Normal";
        case FontSlant::Italic:  return os << "Italic";
        case FontSlant::Oblique: return os << "Oblique";
        default:                 return os << "Slant(" << static_cast<int>(s) << ")";
    }
}

enum class TextDecoration : std::uint8_t {
    None            = 0,
    Underline       = 1 << 0,
    Strikethrough   = 1 << 1,
    Overline        = 1 << 2,
    DoubleUnderline = 1 << 3
};

constexpr TextDecoration operator|(TextDecoration a, TextDecoration b) noexcept {
    return static_cast<TextDecoration>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)
    );
}

constexpr TextDecoration operator&(TextDecoration a, TextDecoration b) noexcept {
    return static_cast<TextDecoration>(
        static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b)
    );
}

constexpr TextDecoration operator~(TextDecoration a) noexcept { return static_cast<TextDecoration>(~static_cast<std::uint8_t>(a)); }
constexpr TextDecoration& operator|=(TextDecoration& a, TextDecoration b) noexcept { a = a | b; return a; }
constexpr TextDecoration& operator&=(TextDecoration& a, TextDecoration b) noexcept { a = a & b; return a; }
constexpr bool has_decoration(TextDecoration flags, TextDecoration test) noexcept { return (flags & test) == test; }

enum class DecorationStyle : std::uint8_t {
    Solid  = 0,
    Dashed = 1,
    Dotted = 2,
    Wavy   = 3,
    Double = 4
};

enum class TextAlign : std::uint8_t {
    Left    = 0,
    Center  = 1,
    Right   = 2,
    Justify = 3
};

inline std::ostream& operator<<(std::ostream& os, TextAlign a) {
    switch (a) {
        case TextAlign::Left:    return os << "Left";
        case TextAlign::Center:  return os << "Center";
        case TextAlign::Right:   return os << "Right";
        case TextAlign::Justify: return os << "Justify";
        default:                 return os << "Align(" << static_cast<int>(a) << ")";
    }
}

enum class VerticalAlign : std::uint8_t {
    Baseline    = 0,
    Superscript = 1,
    Subscript   = 2,
    Top         = 3,
    Middle      = 4,
    Bottom      = 5
};

enum class TextTransform : std::uint8_t {
    None       = 0,
    Uppercase  = 1,
    Lowercase  = 2,
    Capitalize = 3   // first letter of each word
};

enum class WritingDirection : std::uint8_t {
    LeftToRight = 0,
    RightToLeft = 1,
    TopToBottom = 2
};

enum class TextOverflow : std::uint8_t {
    Visible  = 0,   // draw past the bounding box (default)
    Clip     = 1,   // hard clip at the box edge
    Ellipsis = 2,   // truncate with "…"
    WordWrap = 3    // wrap to next line
};

struct TabStop {
    double position = 0.0;                 // pixels from left margin
    TextAlign alignment = TextAlign::Left; // alignment at this stop
    char leader_char = ' ';                // fill character (e.g. '.')
};

struct TextMetrics {
    unsigned int width   = 0;  // total advance width in pixels
    unsigned int height  = 0;  // line height in pixels
    int          ascent  = 0;  // pixels above baseline
    int          descent = 0;  // pixels below baseline (positive downward)
    constexpr bool empty() const noexcept { return width == 0 && height == 0; }
};

namespace detail {
    constexpr double kUnsetDouble = -1.0;
    constexpr double kUnsetSpacing = std::numeric_limits<double>::lowest();
    constexpr bool is_set(double v, double sentinel) noexcept { return v != sentinel; }
}

} // namespace text
} // namespace fizmo

#endif // FIZMO_TEXT_UTIL_HPP