#include "fizmo_library.hpp"
#include "gradient.hpp"

namespace fizmo {
namespace graphics {

auto Gradient::linear(double x0, double y0, double x1, double y1) -> Gradient {
    Gradient g;
    g.m_type = GradientType::Linear;
    g.m_x0 = x0; g.m_y0 = y0;
    g.m_x1 = x1; g.m_y1 = y1;
    g.recompute_linear();
    return g;
}

auto Gradient::radial(double cx, double cy, double radius) -> Gradient {
    Gradient g;
    g.m_type   = GradientType::Radial;
    g.m_x0 = cx; g.m_y0 = cy;
    g.m_radius = (radius > 0.0) ? radius : 1.0;
    g.m_fx = cx;  g.m_fy = cy;
    return g;
}

auto Gradient::set_colors(const Color& start, const Color& end) -> Gradient& {
    m_stops.clear();
    m_stops.emplace_back(0.0, start);
    m_stops.emplace_back(1.0, end);
    m_sorted = true;
    return *this;
}

auto Gradient::set_linear(double x0, double y0, double x1, double y1) noexcept -> Gradient& {
    m_type = GradientType::Linear;
    m_x0 = x0; m_y0 = y0; m_x1 = x1; m_y1 = y1;
    recompute_linear();
    return *this;
}

auto Gradient::set_radial(double cx, double cy, double r) noexcept -> Gradient& {
    m_type = GradientType::Radial;
    m_x0 = cx; m_y0 = cy; m_radius = (r > 0.0) ? r : 1.0;
    if (!m_focal_set) { m_fx = cx; m_fy = cy; }
    return *this;
}

auto Gradient::sample(double x, double y) const noexcept -> Color {
    double t = (m_type == GradientType::Linear) ? project_linear(x, y) : project_radial(x, y);
    t = apply_spread(t);
    return color_at(t);
}

auto Gradient::color_at(double t) const noexcept -> Color {
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

void Gradient::recompute_linear() noexcept {
    m_dx     = m_x1 - m_x0;
    m_dy     = m_y1 - m_y0;
    m_len_sq = m_dx * m_dx + m_dy * m_dy;
    if (m_len_sq <= constants::middle_epsilon()) m_len_sq = constants::middle_epsilon();
}

double Gradient::apply_spread(double t) const noexcept {
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

void Gradient::ensure_sorted() const noexcept {
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

} // namespace graphics
} // namespace fizmo
