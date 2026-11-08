#include "fizmo_library.hpp"
#include "quadtree.hpp"

namespace fizmo {
namespace physics {

bool overlap_circles(
    const RVec2& c1, Real r1,
    const RVec2& c2, Real r2
) noexcept {
    Real dx = c2.x - c1.x;
    Real dy = c2.y - c1.y;
    Real r_sum = r1 + r2;
    return (dx * dx + dy * dy) <= r_sum * r_sum;
}

bool overlap_aabb_circle(
    const AABB& box,
    const RVec2& center,
    Real radius
) noexcept {
    Real cx = std::max(box.min.x, std::min(center.x, box.max.x));
    Real cy = std::max(box.min.y, std::min(center.y, box.max.y));
    Real dx = center.x - cx;
    Real dy = center.y - cy;
    return (dx * dx + dy * dy) <= radius * radius;
}

OverlapResult overlap_aabb_mtv(const AABB& a, const AABB& b) noexcept {
    Real ox = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    Real oy = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    if (ox <= 0.0_r || oy <= 0.0_r) return {};
    OverlapResult r;
    r.hit = true;
    r.depth_x = ox;
    r.depth_y = oy;

    if (ox < oy) {
        Real sign = (a.center().x < b.center().x) ? -1.0_r : 1.0_r;
        r.normal = { sign, 0.0_r };
    } else {
        Real sign = (a.center().y < b.center().y) ? -1.0_r : 1.0_r;
        r.normal = { 0.0_r, sign };
    }

    return r;
}

OverlapResult overlap_circles_mtv(
    const RVec2& c1, Real r1,
    const RVec2& c2, Real r2
) noexcept {
    RVec2 d = c1 - c2;
    Real dist_sq = fast::length_squared(d);
    Real r_sum = r1 + r2;
    if (dist_sq >= r_sum * r_sum) return {};
    OverlapResult r;
    r.hit = true;
    Real dist = std::sqrt(dist_sq);

    if (dist > constants::middle_epsilon()) {
        r.normal = d * (1.0_r / dist);
    } else {
        r.normal = { 0.0_r, 1.0_r };
        dist = 0.0_r;
    }

    Real pen = r_sum - dist;
    r.depth_x = pen * std::abs(r.normal.x);
    r.depth_y = pen * std::abs(r.normal.y);
    return r;
}

OverlapResult overlap_aabb_circle_mtv(
    const AABB& box,
    const RVec2& center,
    Real radius
) noexcept {
    Real cx = std::max(box.min.x, std::min(center.x, box.max.x));
    Real cy = std::max(box.min.y, std::min(center.y, box.max.y));
    Real dx = center.x - cx;
    Real dy = center.y - cy;
    Real dist_sq = dx * dx + dy * dy;
    if (dist_sq > radius * radius) return {};
    OverlapResult r;
    r.hit = true;
    Real dist = std::sqrt(dist_sq);

    if (dist > constants::middle_epsilon()) {
        r.normal = { dx / dist, dy / dist };
        Real pen = radius - dist;
        r.depth_x = pen * std::abs(r.normal.x);
        r.depth_y = pen * std::abs(r.normal.y);
    } else {
        Real left   = center.x - box.min.x;
        Real right  = box.max.x - center.x;
        Real top    = center.y - box.min.y;
        Real bottom = box.max.y - center.y;
        Real min_d  = std::min({ left, right, top, bottom });

        if (min_d == left)        r.normal = { -1.0_r,  0.0_r };
        else if (min_d == right)  r.normal = {  1.0_r,  0.0_r };
        else if (min_d == top)    r.normal = {  0.0_r, -1.0_r };
        else                      r.normal = {  0.0_r,  1.0_r };

        r.depth_x = (min_d + radius) * std::abs(r.normal.x);
        r.depth_y = (min_d + radius) * std::abs(r.normal.y);
    }

    return r;
}

} // namespace physics
} // namespace fizmo
