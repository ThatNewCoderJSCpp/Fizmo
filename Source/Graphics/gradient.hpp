#ifndef FIZMO_GRADIENT_HPP
#define FIZMO_GRADIENT_HPP

#include "color.hpp"
#include "../Basic/constants.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdint>

namespace fizmo {
namespace graphics {

struct GradientStop {
    double position;   // [0, 1]
    Color  color;

    constexpr GradientStop() noexcept : position(0.0) {}
    constexpr GradientStop(double pos, const Color& c) noexcept : position(pos), color(c) {}
};

enum class GradientSpread : std::uint8_t {
    Pad = 0,       // clamp to nearest stop
    Repeat,        // tile
    Reflect        // ping-pong
};

enum class GradientType : std::uint8_t {
    Linear = 0,
    Radial
};

class Gradient {
private:
    GradientType   m_type   = GradientType::Linear;
    GradientSpread m_spread = GradientSpread::Pad;

    double m_x0 = 0.0, m_y0 = 0.0;
    double m_x1 = 1.0, m_y1 = 0.0;

    double m_radius  = 1.0;
    double m_fx = 0.0, m_fy = 0.0;
    bool   m_focal_set = false;

    std::vector<GradientStop> m_stops;
    bool m_sorted = true;

    double m_dx = 1.0, m_dy = 0.0, m_len_sq = 1.0;

public:
    Gradient() = default;

    static Gradient linear(double x0, double y0, double x1, double y1);

    static Gradient radial(double cx, double cy, double radius);

    static Gradient radial(double cx, double cy, double radius, double fx, double fy) {
        Gradient g = radial(cx, cy, radius);
        g.m_fx = fx;  g.m_fy = fy;
        g.m_focal_set = true;
        return g;
    }

    Gradient& add_stop(double position, const Color& color) {
        m_stops.emplace_back(clamp01(position), color);
        m_sorted = false;
        return *this;
    }

    Gradient& clear_stops() { m_stops.clear(); return *this; }
    const std::vector<GradientStop>& stops() const noexcept { return m_stops; }

    Gradient& set_colors(const Color& start, const Color& end);

    Gradient& set_spread(GradientSpread s) noexcept { m_spread = s; return *this; }
    Gradient& set_type(GradientType t) noexcept     { m_type = t; return *this; }

    Gradient& set_linear(double x0, double y0, double x1, double y1) noexcept;

    Gradient& set_radial(double cx, double cy, double r) noexcept;

    Gradient& set_focal(double fx, double fy) noexcept {
        m_fx = fx; m_fy = fy; m_focal_set = true;
        return *this;
    }

    GradientType   type()   const noexcept { return m_type; }
    GradientSpread spread() const noexcept { return m_spread; }

    double start_x()  const noexcept { return m_x0; }
    double start_y()  const noexcept { return m_y0; }
    double end_x()    const noexcept { return m_x1; }
    double end_y()    const noexcept { return m_y1; }
    double center_x() const noexcept { return m_x0; }
    double center_y() const noexcept { return m_y0; }
    double radius()   const noexcept { return m_radius; }
    double focal_x()  const noexcept { return m_fx; }
    double focal_y()  const noexcept { return m_fy; }

    Color sample(double x, double y) const noexcept;

    Color color_at(double t) const noexcept;

private:
    void recompute_linear() noexcept;

    double project_linear(double x, double y) const noexcept {
        return ((x - m_x0) * m_dx + (y - m_y0) * m_dy) / m_len_sq;
    }

    double project_radial(double x, double y) const noexcept {
        double dx = x - m_fx;
        double dy = y - m_fy;
        double dist = std::sqrt(dx * dx + dy * dy);
        return dist / m_radius;
    }

    double apply_spread(double t) const noexcept;

    static double clamp01(double v) noexcept {
        return (v < 0.0) ? 0.0 : (v > 1.0) ? 1.0 : v;
    }

    void ensure_sorted() const noexcept;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_GRADIENT_HPP