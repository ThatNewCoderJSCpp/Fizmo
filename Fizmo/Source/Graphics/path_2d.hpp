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

    Path2D& move_to(double x, double y);

    Path2D& line_to(double x, double y);

    Path2D& quad_to(double cx, double cy, double x, double y);

    Path2D& cubic_to(double c1x, double c1y, double c2x, double c2y, double x, double y);

    Path2D& arc_to(double cx, double cy, double rx, double ry, double start_deg, double sweep_deg);

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

    std::vector<Contour> flatten(double tolerance = 0.5) const;

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
    );

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
    );

    static void flatten_quad(
        std::vector<vector2d>& out,
        vector2d p0, vector2d p1, vector2d p2,
        double tol, int depth = 0
    );

    static void flatten_cubic(
        std::vector<vector2d>& out,
        vector2d p0, vector2d p1, vector2d p2, vector2d p3,
        double tol, int depth = 0
    );

    static vector2d mid(const vector2d& a, const vector2d& b) noexcept {
        return { (a.x + b.x) * 0.5, (a.y + b.y) * 0.5 };
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_PATH_2D_HPP