#ifndef FIZMO_PHYSICS_GJK_2D_HPP
#define FIZMO_PHYSICS_GJK_2D_HPP

#include "physics_2d.hpp"

namespace fizmo {
namespace physics {

struct ConvexProxy2D {
    std::array<RVec2, MAX_POLYGON_VERTICES> points{};
    std::size_t count = 0;
    Real radius = 0.0_r;

    RVec2 support(const RVec2& dir) const noexcept;
    std::size_t support_index(const RVec2& dir) const noexcept;
    RVec2 center() const noexcept;

    static ConvexProxy2D point(const RVec2& p, Real radius = 0.0_r) noexcept;
};

struct DistanceOutput2D {
    RVec2 point_a{};
    RVec2 point_b{};
    RVec2 normal{};
    Real distance = 0.0_r;
    bool overlap = false;
    int iterations = 0;
};

struct CastOutput2D {
    bool hit = false;
    Real fraction = 1.0_r;
    RVec2 point{};
    RVec2 normal{};
};

struct ShapeRayHit2D {
    Real t = 0.0_r;
    RVec2 point{};
    RVec2 normal{};
};

ConvexProxy2D make_convex_proxy(const Shape2D& shape, const Transform2D& xf) noexcept;

DistanceOutput2D gjk_distance(const ConvexProxy2D& a, const ConvexProxy2D& b, int max_iterations = 32) noexcept;

CastOutput2D convex_cast(const ConvexProxy2D& a, const RVec2& translation_a, const ConvexProxy2D& b, Real target = 0.0_r, int max_iterations = 32) noexcept;

bool raycast_shape(const Shape2D& shape, const Transform2D& xf, const RVec2& origin, const RVec2& direction, Real max_distance, ShapeRayHit2D& out) noexcept;

bool overlap_shapes(const Shape2D& a, const Transform2D& xa, const Shape2D& b, const Transform2D& xb) noexcept;

bool cast_shape(const Shape2D& moving, const Transform2D& start, const RVec2& translation, const Shape2D& target, const Transform2D& xt, Real& fraction, RVec2& normal, RVec2& point) noexcept;

Real time_of_impact(const Shape2D& sa, const Sweep2D& wa, const Shape2D& sb, const Sweep2D& wb, const Transform2D& local_a, const Transform2D& local_b, Real target, int max_iterations = 32) noexcept;

Real time_of_impact(const Shape2D& sa, const Sweep2D& wa, const Shape2D& sb, const Sweep2D& wb, const Transform2D& local_a, const Transform2D& local_b, Real target, RVec2& normal, int max_iterations = 32) noexcept;

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_GJK_2D_HPP
