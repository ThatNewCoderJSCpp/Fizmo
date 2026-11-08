#ifndef FIZMO_PHYSICS_CONVEX_3D_HPP
#define FIZMO_PHYSICS_CONVEX_3D_HPP

#include "physics_3d.hpp"

namespace fizmo {
namespace physics {

struct ContactManifold3D;

struct ConvexProxy3D {
    enum class Kind : std::uint8_t {
        Points,
        Cylinder,
        Cone,
        Box
    };

    Kind kind = Kind::Points;
    const RVec3* points = nullptr;
    std::size_t count = 0;
    RVec3 local[3]{};
    RVec3 half_extents{};
    Real half_height = 0.0_r;
    Real core_radius = 0.0_r;
    Real radius = 0.0_r;
    const HullShape* hull = nullptr;
    Transform3D xf{};

    RVec3 support_local(const RVec3& d) const noexcept;
    RVec3 support(const RVec3& world_dir) const noexcept;
    RVec3 center() const noexcept;
    int feature(const RVec3& world_dir, RVec3* out, int capacity) const noexcept;

    static ConvexProxy3D point(const RVec3& p, Real radius = 0.0_r) noexcept;
    static ConvexProxy3D triangle(const RVec3& a, const RVec3& b, const RVec3& c) noexcept;
};

bool make_convex_proxy(const Shape3D& shape, const Transform3D& xf, ConvexProxy3D& out) noexcept;

struct GjkSimplex3D {
    RVec3 w[4]{};
    RVec3 a[4]{};
    RVec3 b[4]{};
    int count = 0;
};

struct GjkOutput3D {
    Real distance = 0.0_r;
    RVec3 point_a{};
    RVec3 point_b{};
    bool overlap = false;
    int iterations = 0;
    GjkSimplex3D simplex{};
};

GjkOutput3D gjk_distance(const ConvexProxy3D& a, const ConvexProxy3D& b, int max_iterations = 64) noexcept;

struct PenetrationOutput3D {
    RVec3 normal{};
    Real depth = 0.0_r;
    RVec3 point_a{};
    RVec3 point_b{};
};

bool epa_penetration(const ConvexProxy3D& a, const ConvexProxy3D& b, const GjkSimplex3D& start, PenetrationOutput3D& out, int max_iterations = 64);

struct ConvexSeparation3D {
    bool valid = false;
    RVec3 normal{};
    Real separation = 0.0_r;
    RVec3 point_a{};
    RVec3 point_b{};
};

ConvexSeparation3D convex_separation(const ConvexProxy3D& a, const ConvexProxy3D& b) noexcept;

bool collide_convex(const ConvexProxy3D& a, const ConvexProxy3D& b, ContactManifold3D& m, Real margin = 0.0_r);

bool collide_convex_plane(const ConvexProxy3D& a, const PlaneShape& p, const Transform3D& xp, ContactManifold3D& m, Real margin = 0.0_r);

bool convex_raycast(const ConvexProxy3D& shape, const RVec3& origin, const RVec3& dir, Real max_t, Real& t, RVec3& normal) noexcept;

bool convex_cast(const ConvexProxy3D& moving, const RVec3& translation, const ConvexProxy3D& target, Real& fraction, RVec3& normal, RVec3& point) noexcept;

bool convex_overlap(const ConvexProxy3D& a, const ConvexProxy3D& b, Real tolerance = 0.0_r) noexcept;

struct ShapeRayHit3D {
    Real t = 0.0_r;
    RVec3 point{};
    RVec3 normal{};
    std::uint32_t feature = 0;
};

bool raycast_shape(const Shape3D& shape, const Transform3D& xf, const RVec3& origin, const RVec3& dir, Real max_t, ShapeRayHit3D& hit) noexcept;

bool overlap_shapes(const Shape3D& a, const Transform3D& xa, const Shape3D& b, const Transform3D& xb, Real tolerance = 0.0_r);

bool cast_shape(const Shape3D& moving, const Transform3D& xa, const RVec3& translation, const Shape3D& target, const Transform3D& xb, Real& fraction, RVec3& normal, RVec3& point);

bool collide_shapes_multi(
    const Shape3D& sa, const Transform3D& xa,
    const Shape3D& sb, const Transform3D& xb,
    Real margin, std::vector<ContactManifold3D>& out
);

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_CONVEX_3D_HPP
