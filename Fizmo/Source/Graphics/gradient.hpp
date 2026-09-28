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

    static Gradient linear(double x0, double y0, double x1, double y1) {
        Gradient g;
        g.m_type = GradientType::Linear;
        g.m_x0 = x0; g.m_y0 = y0;
        g.m_x1 = x1; g.m_y1 = y1;
        g.recompute_linear();
        return g;
    }

    static Gradient radial(double cx, double cy, double radius) {
        Gradient g;
        g.m_type   = GradientType::Radial;
        g.m_x0 = cx; g.m_y0 = cy;
        g.m_radius = (radius > 0.0) ? radius : 1.0;
        g.m_fx = cx;  g.m_fy = cy;
        return g;
    }

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

    Gradient& set_colors(const Color& start, const Color& end) {
        m_stops.clear();
        m_stops.emplace_back(0.0, start);
        m_stops.emplace_back(1.0, end);
        m_sorted = true;
        return *this;
    }

    Gradient& set_spread(GradientSpread s) noexcept { m_spread = s; return *this; }
    Gradient& set_type(GradientType t) noexcept     { m_type = t; return *this; }

    Gradient& set_linear(double x0, double y0, double x1, double y1) noexcept {
        m_type = GradientType::Linear;
        m_x0 = x0; m_y0 = y0; m_x1 = x1; m_y1 = y1;
        recompute_linear();
        return *this;
    }

    Gradient& set_radial(double cx, double cy, double r) noexcept {
        m_type = GradientType::Radial;
        m_x0 = cx; m_y0 = cy; m_radius = (r > 0.0) ? r : 1.0;
        if (!m_focal_set) { m_fx = cx; m_fy = cy; }
        return *this;
    }

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

    Color sample(double x, double y) const noexcept {
        double t = (m_type == GradientType::Linear) ? project_linear(x, y) : project_radial(x, y);
        t = apply_spread(t);
        return color_at(t);
    }

    Color color_at(double t) const noexcept {
        if (m_stops.empty()) return Color();
        ensure_sorted();
        if (m_stops.size() == 1 || t <= m_stops.front().position) return m_stops.front().color;
        if (t >= m_stops.back().position) return m_stops.back().color;
        std::size_t lo = 0, hi = m_stops.size() - 1;

        while (lo + 1 < hi) {
            std::size_t mid = (lo + hi) / 2;
            if (m_stops[mid].position <= t) lo = mid; else hi = mid;
        }

        const auto& a = m_stops[lo];
        const auto& b = m_stops[hi];
        double span = b.position - a.position;
        double local = (span > 0.0) ? (t - a.position) / span : 0.0;
        return a.color.lerp(b.color, local);
    }

private:
    void recompute_linear() noexcept {
        m_dx     = m_x1 - m_x0;
        m_dy     = m_y1 - m_y0;
        m_len_sq = m_dx * m_dx + m_dy * m_dy;
        if (m_len_sq <= constants::middle_epsilon()) m_len_sq = constants::middle_epsilon();
    }

    double project_linear(double x, double y) const noexcept {
        return ((x - m_x0) * m_dx + (y - m_y0) * m_dy) / m_len_sq;
    }

    double project_radial(double x, double y) const noexcept {
        double dx = x - m_fx;
        double dy = y - m_fy;
        double dist = std::sqrt(dx * dx + dy * dy);
        return dist / m_radius;
    }

    double apply_spread(double t) const noexcept {
        switch (m_spread) {
            case GradientSpread::Repeat: return t - std::floor(t);
            case GradientSpread::Reflect: {
                const double m = t - 2.0 * std::floor(t * 0.5);
                return (m > 1.0) ? 2.0 - m : m;
            }
            case GradientSpread::Pad:
            default:
                return clamp01(t);
        }
    }

    static double clamp01(double v) noexcept {
        return (v < 0.0) ? 0.0 : (v > 1.0) ? 1.0 : v;
    }

    void ensure_sorted() const noexcept {
        if (m_sorted) return;
        auto& self = const_cast<Gradient&>(*this);

        std::sort(
            self.m_stops.begin(), self.m_stops.end(),
            [](const GradientStop& a, const GradientStop& b) {
                return a.position < b.position;
            }
        );

        self.m_sorted = true;
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_GRADIENT_HPP