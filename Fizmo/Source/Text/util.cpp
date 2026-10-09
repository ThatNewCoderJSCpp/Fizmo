#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "util.hpp"

namespace fizmo {
namespace text {

std::ostream& operator<<(std::ostream& os, FontWeight w) {
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

std::ostream& operator<<(std::ostream& os, FontSlant s) {
    switch (s) {
        case FontSlant::Normal:  return os << "Normal";
        case FontSlant::Italic:  return os << "Italic";
        case FontSlant::Oblique: return os << "Oblique";
        default:                 return os << "Slant(" << static_cast<int>(s) << ")";
    }
}

std::ostream& operator<<(std::ostream& os, TextAlign a) {
    switch (a) {
        case TextAlign::Left:    return os << "Left";
        case TextAlign::Center:  return os << "Center";
        case TextAlign::Right:   return os << "Right";
        case TextAlign::Justify: return os << "Justify";
        default:                 return os << "Align(" << static_cast<int>(a) << ")";
    }
}

} // namespace text
} // namespace fizmo
