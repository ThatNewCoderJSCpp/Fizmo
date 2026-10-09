#include "fizmo_library.hpp"
#include "collision_2d.hpp"

namespace fizmo {
namespace graphics {
namespace collision {

void Shape::make_ccw() {
    if (m_points.size() < 3) return;
    double area = 0.0;
    for (std::size_t i = 0; i < m_points.size(); ++i) area += cross(m_points[i], m_points[(i + 1) % m_points.size()]);
    if (area < 0.0) std::reverse(m_points.begin(), m_points.end());
}

auto Shape::box(const vector2d& center, double hw, double hh, double angle_degrees) -> Shape {
    const double a = angle_degrees * constants::pi_180();
    const double c = std::cos(a), s = std::sin(a);
    std::vector<vector2d> pts;
    const double lx[4] = { -hw, hw, hw, -hw }, ly[4] = { -hh, -hh, hh, hh };
    for (int i = 0; i < 4; ++i) pts.push_back({ center.x + lx[i] * c - ly[i] * s, center.y + lx[i] * s + ly[i] * c });
    return Shape(std::move(pts));
}

auto Shape::convex_hull(std::vector<vector2d> pts) -> Shape {
    std::sort(pts.begin(), pts.end(), [](const vector2d& a, const vector2d& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    if (pts.size() < 3) return Shape(std::move(pts));
    std::vector<vector2d> hull(2 * pts.size());
    std::size_t k = 0;
    for (std::size_t i = 0; i < pts.size(); ++i) { while (k >= 2 && cross(hull[k - 1] - hull[k - 2], pts[i] - hull[k - 2]) <= 0.0) --k; hull[k++] = pts[i]; }
    for (std::size_t i = pts.size() - 1, t = k + 1; i > 0; --i) { while (k >= t && cross(hull[k - 1] - hull[k - 2], pts[i - 1] - hull[k - 2]) <= 0.0) --k; hull[k++] = pts[i - 1]; }
    hull.resize(k - 1);
    return Shape(std::move(hull));
}

auto Shape::from_sprite(const Sprite& s) -> Shape {
    const SpriteQuad q = s.quad();
    return Shape({ { q.x[0], q.y[0] }, { q.x[1], q.y[1] }, { q.x[2], q.y[2] }, { q.x[3], q.y[3] } });
}

auto Shape::circle_of_sprite(const Sprite& s, double scale) -> Shape {
    const auto b = s.rotated_bounds();
    return circle({ b.x + b.w * 0.5, b.y + b.h * 0.5 }, std::min(s.display_width(), s.display_height()) * scale);
}

auto Shape::center() const noexcept -> vector2d {
    vector2d c{};
    if (m_points.empty()) return c;
    for (const vector2d& p : m_points) c = c + p;
    return c * (1.0 / static_cast<double>(m_points.size()));
}

void Shape::bounds(double& x0, double& y0, double& x1, double& y1) const noexcept {
    x0 = y0 = std::numeric_limits<double>::max();
    x1 = y1 = std::numeric_limits<double>::lowest();
    for (const vector2d& p : m_points) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); }
    x0 -= m_radius; y0 -= m_radius; x1 += m_radius; y1 += m_radius;
}

} // namespace collision
} // namespace graphics
} // namespace fizmo

namespace fizmo {
namespace graphics {
namespace collision {
namespace detail {

vector2d closest_on_segment(const vector2d& p, const vector2d& a, const vector2d& b) noexcept {
    const vector2d ab = b - a;
    const double d = dot(ab, ab);
    if (d <= 0.0) return a;
    const double t = std::max(0.0, std::min(1.0, dot(p - a, ab) / d));
    return a + ab * t;
}

void segment_closest(const vector2d& p1, const vector2d& q1, const vector2d& p2, const vector2d& q2, vector2d& c1, vector2d& c2) noexcept {
    const vector2d d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const double a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
    double s = 0.0, t = 0.0;
    if (a <= 1e-12 && e <= 1e-12) { c1 = p1; c2 = p2; return; }
    if (a <= 1e-12) { t = std::max(0.0, std::min(1.0, f / e)); }
    else {
        const double c = dot(d1, r);
        if (e <= 1e-12) { s = std::max(0.0, std::min(1.0, -c / a)); }
        else {
            const double b = dot(d1, d2), denom = a * e - b * b;
            s = denom != 0.0 ? std::max(0.0, std::min(1.0, (b * f - c * e) / denom)) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) { t = 0.0; s = std::max(0.0, std::min(1.0, -c / a)); }
            else if (t > 1.0) { t = 1.0; s = std::max(0.0, std::min(1.0, (b - c) / a)); }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}

bool core_contains(const std::vector<vector2d>& pts, const vector2d& p) noexcept {
    if (pts.size() < 3) return false;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const vector2d& a = pts[i];
        const vector2d& b = pts[(i + 1) % pts.size()];
        if (cross(b - a, p - a) < 0.0) return false;
    }
    return true;
}

void project(const std::vector<vector2d>& pts, const vector2d& axis, double& lo, double& hi) noexcept {
    lo = hi = dot(pts[0], axis);
    for (std::size_t i = 1; i < pts.size(); ++i) { const double d = dot(pts[i], axis); lo = std::min(lo, d); hi = std::max(hi, d); }
}

bool sat_cores(const std::vector<vector2d>& a, const vector2d& ca, const std::vector<vector2d>& b, const vector2d& cb, vector2d& normal, double& depth) noexcept {
    depth = std::numeric_limits<double>::max();
    auto test = [&](const std::vector<vector2d>& poly) {
        if (poly.size() < 2) return true;
        const std::size_t edges = poly.size() == 2 ? 1 : poly.size();
        for (std::size_t i = 0; i < edges; ++i) {
            const vector2d e = poly[(i + 1) % poly.size()] - poly[i];
            vector2d n{ e.y, -e.x };
            const double l = len(n);
            if (l <= 1e-12) continue;
            n = n * (1.0 / l);
            double a0, a1, b0, b1;
            project(a, n, a0, a1);
            project(b, n, b0, b1);
            if (a1 < b0 || b1 < a0) return false;
            const double push_pos = a1 - b0, push_neg = b1 - a0;
            const double o = std::min(push_pos, push_neg);
            if (o < depth) { depth = o; normal = push_pos <= push_neg ? n : n * -1.0; }
        }
        return true;
    };
    (void)ca;
    (void)cb;
    return test(a) && test(b);
}

double feature_distance(const std::vector<vector2d>& a, const std::vector<vector2d>& b, vector2d& pa, vector2d& pb) noexcept {
    double best = std::numeric_limits<double>::max();
    const std::size_t ea = a.size() <= 2 ? 1 : a.size(), eb = b.size() <= 2 ? 1 : b.size();
    for (std::size_t i = 0; i < ea; ++i) {
        const vector2d& a0 = a[i];
        const vector2d& a1 = a.size() == 1 ? a[0] : a[(i + 1) % a.size()];
        for (std::size_t j = 0; j < eb; ++j) {
            const vector2d& b0 = b[j];
            const vector2d& b1 = b.size() == 1 ? b[0] : b[(j + 1) % b.size()];
            vector2d c1, c2;
            segment_closest(a0, a1, b0, b1, c1, c2);
            const double d = len(c2 - c1);
            if (d < best) { best = d; pa = c1; pb = c2; }
        }
    }
    return best;
}

bool cores_overlap(const Shape& a, const Shape& b, double feature, vector2d& normal, double& depth) noexcept {
    const auto& pa = a.points();
    const auto& pb = b.points();
    if (pa.size() < 3 && pb.size() < 3) {
        if (feature > 1e-9) return false;
        const vector2d dc = b.center() - a.center();
        const double l = len(dc);
        normal = l > 1e-12 ? dc * (1.0 / l) : vector2d(1.0, 0.0);
        depth = 0.0;
        return true;
    }
    if (!sat_cores(pa, a.center(), pb, b.center(), normal, depth)) return false;
    if (depth == std::numeric_limits<double>::max()) depth = 0.0;
    return true;
}

} // namespace detail
} // namespace collision
} // namespace graphics
} // namespace fizmo

namespace fizmo {
namespace graphics {
namespace collision {

double distance(const Shape& a, const Shape& b) noexcept {
    if (a.empty() || b.empty()) return std::numeric_limits<double>::max();
    vector2d ca, cb, n;
    double depth = 0.0;
    const double d = detail::feature_distance(a.points(), b.points(), ca, cb);
    if (detail::cores_overlap(a, b, d, n, depth)) return -(depth + a.radius() + b.radius());
    return d - a.radius() - b.radius();
}

Hit overlap(const Shape& a, const Shape& b) noexcept {
    Hit h;
    if (a.empty() || b.empty()) return h;
    const double ra = a.radius(), rb = b.radius();
    vector2d ca, cb, n;
    double depth = 0.0;
    const double d = detail::feature_distance(a.points(), b.points(), ca, cb);

    if (detail::cores_overlap(a, b, d, n, depth)) {
        h.hit = true;
        h.normal = n;
        h.depth = depth + ra + rb;
        h.point = (ca + cb) * 0.5;
        return h;
    }

    if (d >= ra + rb) return h;
    h.hit = true;
    h.normal = d > 1e-12 ? (cb - ca) * (1.0 / d) : vector2d(1.0, 0.0);
    h.depth = ra + rb - d;
    h.point = ca + h.normal * (ra - h.depth * 0.5);
    return h;
}

bool contains(const Shape& s, const vector2d& p) noexcept {
    if (s.empty()) return false;
    if (s.points().size() >= 3 && detail::core_contains(s.points(), p)) return true;
    const Shape pt = Shape::point(p);
    vector2d a, b;
    return detail::feature_distance(s.points(), pt.points(), a, b) <= s.radius();
}

RayHit raycast(const Shape& s, const vector2d& origin, const vector2d& dir, double max_t) noexcept {
    RayHit best;
    if (s.empty()) return best;
    const double dl = detail::len(dir);
    if (dl <= 1e-12) return best;
    const vector2d d = dir * (1.0 / dl);
    if (contains(s, origin)) { best.hit = true; best.t = 0.0; best.point = origin; best.normal = d * -1.0; return best; }
    best.t = max_t;
    const auto& pts = s.points();
    const double r = s.radius();

    auto try_circle = [&](const vector2d& c) {
        if (r <= 0.0) return;
        const vector2d m = origin - c;
        const double b = detail::dot(m, d), cc = detail::dot(m, m) - r * r;
        if (cc > 0.0 && b > 0.0) return;
        const double disc = b * b - cc;
        if (disc < 0.0) return;
        const double t = -b - std::sqrt(disc);
        if (t >= 0.0 && t < best.t) { best.hit = true; best.t = t; best.point = origin + d * t; best.normal = (best.point - c) * (1.0 / r); }
    };

    auto try_segment = [&](const vector2d& a, const vector2d& b, const vector2d& outward) {
        const vector2d off = outward * r;
        const vector2d a2 = a + off, b2 = b + off;
        const vector2d e = b2 - a2;
        const double denom = detail::cross(d, e);
        if (std::abs(denom) <= 1e-12) return;
        const vector2d w = a2 - origin;
        const double t = detail::cross(w, e) / denom;
        const double u = detail::cross(w, d) / denom;
        if (t >= 0.0 && u >= 0.0 && u <= 1.0 && t < best.t && detail::dot(d, outward) < 0.0) { best.hit = true; best.t = t; best.point = origin + d * t; best.normal = outward; }
    };

    if (pts.size() == 1) {
        try_circle(pts[0]);
    } else {
        const std::size_t edges = pts.size() == 2 ? 2 : pts.size();
        for (std::size_t i = 0; i < edges; ++i) {
            const vector2d& a = pts[i % pts.size()];
            const vector2d& b = pts[(i + 1) % pts.size()];
            const vector2d e = b - a;
            const double l = detail::len(e);
            if (l <= 1e-12) continue;
            const vector2d outward{ e.y / l, -e.x / l };
            try_segment(a, b, outward);
        }
        for (const vector2d& p : pts) try_circle(p);
    }

    if (!best.hit) best.t = 0.0;
    return best;
}

SweepHit sweep(const Shape& moving, const vector2d& delta, const Shape& target, int iterations) noexcept {
    SweepHit out;
    const double dl = detail::len(delta);
    double t = 0.0;
    if (distance(moving, target) <= 0.0) { out.hit = true; out.time = 0.0; out.normal = overlap(moving, target).normal * -1.0; return out; }
    if (dl <= 1e-12) return out;

    for (int i = 0; i < iterations; ++i) {
        const Shape at = moving.translated(delta * t);
        const double d = distance(at, target);
        if (d <= 1e-6) {
            out.hit = true;
            out.time = t;
            vector2d pa, pb;
            detail::feature_distance(at.points(), target.points(), pa, pb);
            const vector2d n = pa - pb;
            const double l = detail::len(n);
            out.normal = l > 1e-12 ? n * (1.0 / l) : delta * (-1.0 / dl);
            out.point = pb + out.normal * target.radius();
            return out;
        }
        t += d / dl;
        if (t > 1.0) return out;
    }

    return out;
}

} // namespace collision
} // namespace graphics
} // namespace fizmo
