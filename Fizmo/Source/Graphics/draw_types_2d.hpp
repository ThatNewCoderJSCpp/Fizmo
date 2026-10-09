#ifndef FIZMO_DRAW_TYPES_2D_HPP
#define FIZMO_DRAW_TYPES_2D_HPP

#include "color.hpp"
#include <cstdint>

namespace fizmo {
namespace graphics {

enum class BlendMode : std::uint8_t { Normal = 0, Add, Multiply, Screen, Count };

inline const char* blend_mode_name(BlendMode m) noexcept {
    switch (m) {
        case BlendMode::Normal:   return "Normal";
        case BlendMode::Add:      return "Add";
        case BlendMode::Multiply: return "Multiply";
        case BlendMode::Screen:   return "Screen";
        default:                  return "";
    }
}

struct Vertex2D {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    Color color = Color(255, 255, 255, 255);

    constexpr Vertex2D() noexcept = default;
    constexpr Vertex2D(float px, float py, const Color& c) noexcept : x(px), y(py), color(c) {}
    constexpr Vertex2D(float px, float py, float pu, float pv, const Color& c) noexcept : x(px), y(py), u(pu), v(pv), color(c) {}
};

inline bool is_white(const Color& c) noexcept { return c.red() == 255 && c.green() == 255 && c.blue() == 255 && c.alpha() == 255; }

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_DRAW_TYPES_2D_HPP
