#include "fizmo_library.hpp"
#include "draw_types_2d.hpp"

namespace fizmo {
namespace graphics {

const char* blend_mode_name(BlendMode m) noexcept {
    switch (m) {
        case BlendMode::Normal:   return "Normal";
        case BlendMode::Add:      return "Add";
        case BlendMode::Multiply: return "Multiply";
        case BlendMode::Screen:   return "Screen";
        default:                  return "";
    }
}

} // namespace graphics
} // namespace fizmo
