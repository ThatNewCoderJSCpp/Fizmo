#include "fizmo_library.hpp"
#include "octree.hpp"

namespace fizmo {
namespace physics {

bool overlap_aabb_sphere(
    const AABB3D& box,
    const RVec3& center,
    Real radius
) noexcept {
    RVec3 closest = vec3::max(box.min, vec3::min(center, box.max));
    return vec3::length_squared(center - closest) <= radius * radius;
}

OverlapResult3D overlap_aabb_mtv(const AABB3D& a, const AABB3D& b) noexcept {
    const Real o[3] = {
        std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x),
        std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y),
        std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z)
    };

    if (o[0] <= 0.0_r || o[1] <= 0.0_r || o[2] <= 0.0_r) return {};
    int axis = 0;
    if (o[1] < o[axis]) axis = 1;
    if (o[2] < o[axis]) axis = 2;
    OverlapResult3D r;
    r.hit = true;
    r.depth = o[axis];
    Real sign = (vec3::component(a.center(), axis) < vec3::component(b.center(), axis)) ? -1.0_r : 1.0_r;
    r.normal = { axis == 0 ? sign : 0.0_r, axis == 1 ? sign : 0.0_r, axis == 2 ? sign : 0.0_r };
    return r;
}

OverlapResult3D overlap_spheres_mtv(
    const RVec3& c1, Real r1,
    const RVec3& c2, Real r2
) noexcept {
    RVec3 d = c1 - c2;
    Real dist_sq = vec3::length_squared(d);
    Real r_sum = r1 + r2;
    if (dist_sq >= r_sum * r_sum) return {};
    OverlapResult3D r;
    r.hit = true;
    Real dist = std::sqrt(dist_sq);

    if (dist > constants::middle_epsilon()) {
        r.normal = d * (1.0_r / dist);
    } else {
        r.normal = { 0.0_r, 1.0_r, 0.0_r };
        dist = 0.0_r;
    }

    r.depth = r_sum - dist;
    return r;
}

OverlapResult3D overlap_aabb_sphere_mtv(
    const AABB3D& box,
    const RVec3& center,
    Real radius
) noexcept {
    RVec3 closest = vec3::max(box.min, vec3::min(center, box.max));
    RVec3 d = center - closest;
    Real dist_sq = vec3::length_squared(d);
    if (dist_sq > radius * radius) return {};
    OverlapResult3D r;
    r.hit = true;
    Real dist = std::sqrt(dist_sq);

    if (dist > constants::middle_epsilon()) {
        r.normal = d * (1.0_r / dist);
        r.depth = radius - dist;
    } else {
        const Real gaps[6] = {
            center.x - box.min.x, box.max.x - center.x,
            center.y - box.min.y, box.max.y - center.y,
            center.z - box.min.z, box.max.z - center.z
        };

        int best = 0;
        for (int i = 1; i < 6; ++i) if (gaps[i] < gaps[best]) best = i;
        const Real s = (best % 2 == 0) ? -1.0_r : 1.0_r;
        const int axis = best / 2;
        r.normal = { axis == 0 ? s : 0.0_r, axis == 1 ? s : 0.0_r, axis == 2 ? s : 0.0_r };
        r.depth = gaps[best] + radius;
    }

    return r;
}

} // namespace physics
} // namespace fizmo
