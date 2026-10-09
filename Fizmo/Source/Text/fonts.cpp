#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "fonts.hpp"

namespace fizmo {
namespace text {

std::ostream& operator<<(std::ostream& os, FontCategory f) {
    switch (f) {
        case FontCategory::Default:   return os << "Default";
        case FontCategory::SansSerif: return os << "SansSerif";
        case FontCategory::Serif:     return os << "Serif";
        case FontCategory::Monospace: return os << "Monospace";
        case FontCategory::Cursive:   return os << "Cursive";
        case FontCategory::Fantasy:   return os << "Fantasy";
        case FontCategory::System:    return os << "System";
        default:                      return os << "FontFamily(" << static_cast<int>(f) << ")";
    }
}

FontCategory font_category(Font f) noexcept {
    switch (f) {
        FIZMO_SERIF_FONTS(FIZMO_ENUM_CAT)
            return FontCategory::Serif;

        FIZMO_SANS_SERIF_FONTS(FIZMO_ENUM_CAT)
            return FontCategory::SansSerif;

        FIZMO_MONOSPACE_FONTS(FIZMO_ENUM_CAT)
            return FontCategory::Monospace;

        FIZMO_DISPLAY_FONTS(FIZMO_ENUM_CAT)
            return FontCategory::Fantasy;

        FIZMO_HANDWRITING_FONTS(FIZMO_ENUM_CAT)
            return FontCategory::Cursive;

        default:
            return FontCategory::Default;
    }
}

SerifFont to_serif(Font f) noexcept {
    switch (f) {
        FIZMO_SERIF_FONTS(FIZMO_FROM_FONT_SERIF)
        default: return SerifFont::InvalidSerif;
    }
}

SansSerifFont to_sans_serif(Font f) noexcept {
    switch (f) {
        FIZMO_SANS_SERIF_FONTS(FIZMO_FROM_FONT_SANS)
        default: return SansSerifFont::InvalidSansSerif;
    }
}

MonospaceFont to_monospace(Font f) noexcept {
    switch (f) {
        FIZMO_MONOSPACE_FONTS(FIZMO_FROM_FONT_MONO)
        default: return MonospaceFont::InvalidMonospace;
    }
}

DisplayFont to_display(Font f) noexcept {
    switch (f) {
        FIZMO_DISPLAY_FONTS(FIZMO_FROM_FONT_DISPLAY)
        default: return DisplayFont::InvalidDisplay;
    }
}

HandwritingFont to_handwriting(Font f) noexcept {
    switch (f) {
        FIZMO_HANDWRITING_FONTS(FIZMO_FROM_FONT_HANDWRITING)
        default: return HandwritingFont::InvalidCursive;
    }
}

std::ostream& operator<<(std::ostream& os, Font f) {
    if (f == Font::None) return os << "None";
    const char* n = font_css_name(f);
    return (n[0] != '\0') ? (os << n) : (os << "Font(" << static_cast<int>(f) << ")");
}

} // namespace text
} // namespace fizmo
