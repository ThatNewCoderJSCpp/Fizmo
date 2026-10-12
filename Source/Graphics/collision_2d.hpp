#ifndef FIZMO_COLLISION_2D_HPP
#define FIZMO_COLLISION_2D_HPP

#include "sprite.hpp"
#include "../Vectors/vectors.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace fizmo {
namespace graphics {
namespace collision {

struct Hit {
    bool     hit = false;
    linalg::vector2d normal{};
    double   depth = 0.0;
    linalg::vector2d point{};
};

struct RayHit {
    bool     hit = false;
    double   t = 0.0;
    linalg::vector2d point{};
    linalg::vector2d normal{};
};

struct SweepHit {
    bool     hit = false;
    double   time = 1.0;
    linalg::vector2d normal{};
    linalg::vector2d point{};
};

class Shape {
private:
    std::vector<linalg::vector2d> m_points;
    double                m_radius = 0.0;

    static double cross(const linalg::vector2d& a, const linalg::vector2d& b) noexcept { return a.x * b.y - a.y * b.x; }

    void make_ccw();

public:
    Shape() = default;
    Shape(std::vector<linalg::vector2d> pts, double radius = 0.0) : m_points(std::move(pts)), m_radius(std::max(0.0, radius)) { make_ccw(); }

    static Shape circle(const linalg::vector2d& c, double r) { return Shape({ c }, r); }
    static Shape capsule(const linalg::vector2d& a, const linalg::vector2d& b, double r) { return Shape({ a, b }, r); }
    static Shape segment(const linalg::vector2d& a, const linalg::vector2d& b) { return Shape({ a, b }, 0.0); }
    static Shape point(const linalg::vector2d& p) { return Shape({ p }, 0.0); }
    static Shape rect(double x, double y, double w, double h) { return Shape({ { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h } }); }

    static Shape box(const linalg::vector2d& center, double hw, double hh, double angle_degrees = 0.0);

    static Shape polygon(std::vector<linalg::vector2d> pts) { return Shape(std::move(pts)); }

    static Shape convex_hull(std::vector<linalg::vector2d> pts);

    static Shape from_sprite(const Sprite& s);

    static Shape circle_of_sprite(const Sprite& s, double scale = 0.5);

    const std::vector<linalg::vector2d>& points() const noexcept { return m_points; }
    double radius() const noexcept { return m_radius; }
    bool empty() const noexcept { return m_points.empty(); }

    Shape translated(const linalg::vector2d& d) const {
        Shape s = *this;
        for (linalg::vector2d& p : s.m_points) p = p + d;
        return s;
    }

    linalg::vector2d center() const noexcept;

    void bounds(double& x0, double& y0, double& x1, double& y1) const noexcept;
};

namespace detail {

inline double dot(const linalg::vector2d& a, const linalg::vector2d& b) noexcept { return a.x * b.x + a.y * b.y; }
inline double cross(const linalg::vector2d& a, const linalg::vector2d& b) noexcept { return a.x * b.y - a.y * b.x; }
inline double len(const linalg::vector2d& a) noexcept { return std::sqrt(dot(a, a)); }

linalg::vector2d closest_on_segment(const linalg::vector2d& p, const linalg::vector2d& a, const linalg::vector2d& b) noexcept;

void segment_closest(const linalg::vector2d& p1, const linalg::vector2d& q1, const linalg::vector2d& p2, const linalg::vector2d& q2, linalg::vector2d& c1, linalg::vector2d& c2) noexcept;

bool core_contains(const std::vector<linalg::vector2d>& pts, const linalg::vector2d& p) noexcept;

void project(const std::vector<linalg::vector2d>& pts, const linalg::vector2d& axis, double& lo, double& hi) noexcept;

bool sat_cores(const std::vector<linalg::vector2d>& a, const linalg::vector2d& ca, const std::vector<linalg::vector2d>& b, const linalg::vector2d& cb, linalg::vector2d& normal, double& depth) noexcept;

double feature_distance(const std::vector<linalg::vector2d>& a, const std::vector<linalg::vector2d>& b, linalg::vector2d& pa, linalg::vector2d& pb) noexcept;

} // namespace detail

namespace detail {

bool cores_overlap(const Shape& a, const Shape& b, double feature, linalg::vector2d& normal, double& depth) noexcept;

} // namespace detail

double distance(const Shape& a, const Shape& b) noexcept;

Hit overlap(const Shape& a, const Shape& b) noexcept;

inline bool intersects(const Shape& a, const Shape& b) noexcept { return overlap(a, b).hit; }

bool contains(const Shape& s, const linalg::vector2d& p) noexcept;

RayHit raycast(const Shape& s, const linalg::vector2d& origin, const linalg::vector2d& dir, double max_t = std::numeric_limits<double>::max()) noexcept;

SweepHit sweep(const Shape& moving, const linalg::vector2d& delta, const Shape& target, int iterations = 32) noexcept;

inline Hit collide(const Sprite& a, const Sprite& b) { return overlap(Shape::from_sprite(a), Shape::from_sprite(b)); }
inline Hit collide(const Sprite& a, const Shape& b) { return overlap(Shape::from_sprite(a), b); }
inline bool contains(const Sprite& s, double x, double y) { return contains(Shape::from_sprite(s), { x, y }); }
inline RayHit raycast(const Sprite& s, const linalg::vector2d& origin, const linalg::vector2d& dir, double max_t = std::numeric_limits<double>::max()) { return raycast(Shape::from_sprite(s), origin, dir, max_t); }

} // namespace collision
} // namespace graphics
} // namespace fizmo

#endif // FIZMO_COLLISION_2D_HPP
