#include "fizmo_library.hpp"
#include "octree.hpp"

namespace fizmo {
namespace physics {

bool overlap_aabb_sphere(
    const AABB3D& box,
    const vector3d& center,
    double radius
) noexcept {
    vector3d closest = vec3::max(box.min, vec3::min(center, box.max));
    return vec3::length_squared(center - closest) <= radius * radius;
}

OverlapResult3D overlap_aabb_mtv(const AABB3D& a, const AABB3D& b) noexcept {
    const double o[3] = {
        std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x),
        std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y),
        std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z)
    };

    if (o[0] <= 0.0 || o[1] <= 0.0 || o[2] <= 0.0) return {};
    int axis = 0;
    if (o[1] < o[axis]) axis = 1;
    if (o[2] < o[axis]) axis = 2;
    OverlapResult3D r;
    r.hit = true;
    r.depth = o[axis];
    double sign = (vec3::component(a.center(), axis) < vec3::component(b.center(), axis)) ? -1.0 : 1.0;
    r.normal = { axis == 0 ? sign : 0.0, axis == 1 ? sign : 0.0, axis == 2 ? sign : 0.0 };
    return r;
}

OverlapResult3D overlap_spheres_mtv(
    const vector3d& c1, double r1,
    const vector3d& c2, double r2
) noexcept {
    vector3d d = c1 - c2;
    double dist_sq = vec3::length_squared(d);
    double r_sum = r1 + r2;
    if (dist_sq >= r_sum * r_sum) return {};
    OverlapResult3D r;
    r.hit = true;
    double dist = std::sqrt(dist_sq);

    if (dist > constants::middle_epsilon()) {
        r.normal = d * (1.0 / dist);
    } else {
        r.normal = { 0.0, 1.0, 0.0 };
        dist = 0.0;
    }

    r.depth = r_sum - dist;
    return r;
}

OverlapResult3D overlap_aabb_sphere_mtv(
    const AABB3D& box,
    const vector3d& center,
    double radius
) noexcept {
    vector3d closest = vec3::max(box.min, vec3::min(center, box.max));
    vector3d d = center - closest;
    double dist_sq = vec3::length_squared(d);
    if (dist_sq > radius * radius) return {};
    OverlapResult3D r;
    r.hit = true;
    double dist = std::sqrt(dist_sq);

    if (dist > constants::middle_epsilon()) {
        r.normal = d * (1.0 / dist);
        r.depth = radius - dist;
    } else {
        const double gaps[6] = {
            center.x - box.min.x, box.max.x - center.x,
            center.y - box.min.y, box.max.y - center.y,
            center.z - box.min.z, box.max.z - center.z
        };

        int best = 0;
        for (int i = 1; i < 6; ++i) if (gaps[i] < gaps[best]) best = i;
        const double s = (best % 2 == 0) ? -1.0 : 1.0;
        const int axis = best / 2;
        r.normal = { axis == 0 ? s : 0.0, axis == 1 ? s : 0.0, axis == 2 ? s : 0.0 };
        r.depth = gaps[best] + radius;
    }

    return r;
}

} // namespace physics
} // namespace fizmo
