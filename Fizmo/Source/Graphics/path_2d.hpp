#ifndef FIZMO_PATH_2D_HPP
#define FIZMO_PATH_2D_HPP

#include <vector>
#include <cmath>
#include <utility>
#include <cstdint>
#include <algorithm>
#include "../Basic/constants.hpp"
#include "../Vectors/vectors.hpp"

namespace fizmo {
namespace graphics {

class Path2D {
public:
    enum class Verb : std::uint8_t { Move = 0, Line, Quad, Cubic, Close };
    Path2D() noexcept = default;

    Path2D& move_to(double x, double y) {
        m_verbs.push_back(Verb::Move);
        m_pts.push_back({ x, y });
        m_cursor = { x, y };
        m_subpath_start = m_cursor;
        return *this;
    }

    Path2D& line_to(double x, double y) {
        ensure_begun();
        m_verbs.push_back(Verb::Line);
        m_pts.push_back({ x, y });
        m_cursor = { x, y };
        return *this;
    }

    Path2D& quad_to(double cx, double cy, double x, double y) {
        ensure_begun();
        m_verbs.push_back(Verb::Quad);
        m_pts.push_back({ cx, cy });
        m_pts.push_back({ x, y });
        m_cursor = { x, y };
        return *this;
    }

    Path2D& cubic_to(double c1x, double c1y, double c2x, double c2y, double x, double y) {
        ensure_begun();
        m_verbs.push_back(Verb::Cubic);
        m_pts.push_back({ c1x, c1y });
        m_pts.push_back({ c2x, c2y });
        m_pts.push_back({ x, y });
        m_cursor = { x, y };
        return *this;
    }

    Path2D& arc_to(double cx, double cy, double rx, double ry, double start_deg, double sweep_deg) {
        if (std::abs(sweep_deg) <= constants::epsilon()) return *this;
        constexpr double kDeg2Rad = constants::pi_180();
        double start = start_deg * kDeg2Rad;
        double sweep = sweep_deg * kDeg2Rad;
        int n = static_cast<int>(std::ceil(std::abs(sweep) * 0.5 * constants::reciprocal_pi()));
        if (n < 1) n = 1;
        double step = sweep / n;

        for (int i = 0; i < n; ++i) {
            double a0 = start + i * step;
            double a1 = a0 + step;
            arc_segment_cubic(cx, cy, rx, ry, a0, a1, i == 0);
        }

        return *this;
    }

    Path2D& close() {
        m_verbs.push_back(Verb::Close);
        m_cursor = m_subpath_start;
        return *this;
    }

    bool empty() const noexcept { return m_verbs.empty(); }
    std::size_t verb_count()  const noexcept { return m_verbs.size(); }
    std::size_t point_count() const noexcept { return m_pts.size(); }

    void clear() noexcept {
        m_verbs.clear();
        m_pts.clear();
        m_cursor = { 0, 0 };
        m_subpath_start = { 0, 0 };
    }

    void reserve(std::size_t verbs, std::size_t pts) {
        m_verbs.reserve(verbs);
        m_pts.reserve(pts);
    }

    struct Contour {
        std::vector<vector2d> points;
        bool closed = false;
    };

    std::vector<Contour> flatten(double tolerance = 0.5) const {
        std::vector<Contour> contours;
        Contour current;
        vector2d pen{ 0, 0 };
        std::size_t pi = 0; 

        auto finish_contour = [&]() {
            if (current.points.size() >= 2) contours.push_back(std::move(current));
            current = Contour{};
        };

        for (auto verb : m_verbs) {
            switch (verb) {

            case Verb::Move:
                finish_contour();
                pen = m_pts[pi++];
                current.points.push_back(pen);
                break;

            case Verb::Line:
                pen = m_pts[pi++];
                current.points.push_back(pen);
                break;

            case Verb::Quad: {
                vector2d cp  = m_pts[pi++];
                vector2d end = m_pts[pi++];
                flatten_quad(current.points, pen, cp, end, tolerance);
                pen = end;
                break;
            }

            case Verb::Cubic: {
                vector2d c1  = m_pts[pi++];
                vector2d c2  = m_pts[pi++];
                vector2d end = m_pts[pi++];
                flatten_cubic(current.points, pen, c1, c2, end, tolerance);
                pen = end;
                break;
            }

            case Verb::Close:
                current.closed = true;
                if (!current.points.empty()) pen = current.points.front();
                finish_contour();
                break;
            }
        }

        finish_contour();
        return contours;
    }

    std::vector<vector2d> flatten_single(double tolerance = 0.5) const {
        auto c = flatten(tolerance);
        if (c.empty()) return {};
        return std::move(c[0].points);
    }

    static Path2D rounded_rect(double x, double y, double w, double h, double radius) {
        return rounded_rect(x, y, w, h, radius, radius, radius, radius);
    }

    static Path2D rounded_rect(
        double x, double y,
        double w, double h,
        double r_tl, double r_tr,
        double r_br, double r_bl
    ) {
        double half = std::min(w, h) * 0.5;
        r_tl = std::min(r_tl, half);
        r_tr = std::min(r_tr, half);
        r_br = std::min(r_br, half);
        r_bl = std::min(r_bl, half);
        Path2D p;
        p.move_to(x + r_tl, y);
        p.line_to(x + w - r_tr, y);
        if (r_tr > 0) p.arc_to(x + w - r_tr, y + r_tr, r_tr, r_tr, -90.0, 90.0);
        p.line_to(x + w, y + h - r_br);
        if (r_br > 0) p.arc_to(x + w - r_br, y + h - r_br, r_br, r_br, 0.0, 90.0);
        p.line_to(x + r_bl, y + h);
        if (r_bl > 0) p.arc_to(x + r_bl, y + h - r_bl, r_bl, r_bl, 90.0, 90.0);
        p.line_to(x, y + r_tl);
        if (r_tl > 0) p.arc_to(x + r_tl, y + r_tl, r_tl, r_tl, 180.0, 90.0);
        p.close();
        return p;
    }

    static Path2D circle(double cx, double cy, double r) {
        Path2D p;
        p.move_to(cx + r, cy);
        p.arc_to(cx, cy, r, r, 0.0, 360.0);
        p.close();
        return p;
    }

    static Path2D ellipse(double cx, double cy, double rx, double ry) {
        Path2D p;
        p.move_to(cx + rx, cy);
        p.arc_to(cx, cy, rx, ry, 0.0, 360.0);
        p.close();
        return p;
    }

private:
    std::vector<Verb>  m_verbs;
    std::vector<vector2d> m_pts;
    vector2d m_cursor{ 0, 0 };
    vector2d m_subpath_start{ 0, 0 };

private:
    void ensure_begun() { if (m_verbs.empty()) move_to(0, 0); }

    void arc_segment_cubic(
        double cx, double cy,
        double rx, double ry,
        double a0, double a1,
        bool first_segment
    ) {
        double da = a1 - a0;
        double alpha = std::sin(da) * (std::sqrt(4.0 + 3.0 * std::tan(da * 0.5) * std::tan(da * 0.5)) - 1.0) / 3.0;
        double cos0 = std::cos(a0), sin0 = std::sin(a0);
        double cos1 = std::cos(a1), sin1 = std::sin(a1);
        double x0 = cx + rx * cos0;
        double y0 = cy + ry * sin0;
        double x3 = cx + rx * cos1;
        double y3 = cy + ry * sin1;
        double dx0 = -rx * sin0;
        double dy0 =  ry * cos0;
        double dx1 = -rx * sin1;
        double dy1 =  ry * cos1;
        double x1 = x0 + alpha * dx0;
        double y1 = y0 + alpha * dy0;
        double x2 = x3 - alpha * dx1;
        double y2 = y3 - alpha * dy1;

        if (first_segment && m_verbs.empty()) {
            move_to(x0, y0);
        } else if (first_segment) {
            line_to(x0, y0);
        }

        cubic_to(x1, y1, x2, y2, x3, y3);
    }

    static void flatten_quad(
        std::vector<vector2d>& out,
        vector2d p0, vector2d p1, vector2d p2,
        double tol, int depth = 0
    ) {
        double dx = p2.x - p0.x;
        double dy = p2.y - p0.y;
        double d = std::abs((p1.x - p2.x) * dy - (p1.y - p2.y) * dx);
        double len_sq = dx * dx + dy * dy;

        if (d * d <= tol * tol * len_sq || depth > 12) {
            out.push_back(p2);
            return;
        }

        vector2d m01 = mid(p0, p1);
        vector2d m12 = mid(p1, p2);
        vector2d m   = mid(m01, m12);
        flatten_quad(out, p0,  m01, m,  tol, depth + 1);
        flatten_quad(out, m,   m12, p2, tol, depth + 1);
    }

    static void flatten_cubic(
        std::vector<vector2d>& out,
        vector2d p0, vector2d p1, vector2d p2, vector2d p3,
        double tol, int depth = 0
    ) {
        double dx = p3.x - p0.x;
        double dy = p3.y - p0.y;
        double d1 = std::abs((p1.x - p3.x) * dy - (p1.y - p3.y) * dx);
        double d2 = std::abs((p2.x - p3.x) * dy - (p2.y - p3.y) * dx);
        double d = std::max(d1, d2);
        double len_sq = dx * dx + dy * dy;

        if (d * d <= tol * tol * len_sq || depth > 12) {
            out.push_back(p3);
            return;
        }

        vector2d m01  = mid(p0, p1);
        vector2d m12  = mid(p1, p2);
        vector2d m23  = mid(p2, p3);
        vector2d m012 = mid(m01, m12);
        vector2d m123 = mid(m12, m23);
        vector2d m    = mid(m012, m123);
        flatten_cubic(out, p0, m01,  m012, m,  tol, depth + 1);
        flatten_cubic(out, m,  m123, m23,  p3, tol, depth + 1);
    }

    static vector2d mid(const vector2d& a, const vector2d& b) noexcept {
        return { (a.x + b.x) * 0.5, (a.y + b.y) * 0.5 };
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_PATH_2D_HPP