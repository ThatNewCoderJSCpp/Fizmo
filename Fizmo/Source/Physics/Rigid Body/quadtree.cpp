#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "quadtree.hpp"

namespace fizmo {
namespace physics {

bool overlap_circles(
    const vector2d& c1, double r1,
    const vector2d& c2, double r2
) noexcept {
    double dx = c2.x - c1.x;
    double dy = c2.y - c1.y;
    double r_sum = r1 + r2;
    return (dx * dx + dy * dy) <= r_sum * r_sum;
}

bool overlap_aabb_circle(
    const AABB& box,
    const vector2d& center,
    double radius
) noexcept {
    double cx = std::max(box.min.x, std::min(center.x, box.max.x));
    double cy = std::max(box.min.y, std::min(center.y, box.max.y));
    double dx = center.x - cx;
    double dy = center.y - cy;
    return (dx * dx + dy * dy) <= radius * radius;
}

OverlapResult overlap_aabb_mtv(const AABB& a, const AABB& b) noexcept {
    double ox = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    double oy = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    if (ox <= 0.0 || oy <= 0.0) return {};
    OverlapResult r;
    r.hit = true;
    r.depth_x = ox;
    r.depth_y = oy;

    if (ox < oy) {
        double sign = (a.center().x < b.center().x) ? -1.0 : 1.0;
        r.normal = { sign, 0.0 };
    } else {
        double sign = (a.center().y < b.center().y) ? -1.0 : 1.0;
        r.normal = { 0.0, sign };
    }

    return r;
}

OverlapResult overlap_circles_mtv(
    const vector2d& c1, double r1,
    const vector2d& c2, double r2
) noexcept {
    vector2d d = c1 - c2;
    double dist_sq = d.magnitude_squared();
    double r_sum = r1 + r2;
    if (dist_sq >= r_sum * r_sum) return {};
    OverlapResult r;
    r.hit = true;
    double dist = std::sqrt(dist_sq);

    if (dist > constants::middle_epsilon()) {
        r.normal = d * (1.0 / dist);
    } else {
        r.normal = { 0.0, 1.0 };
        dist = 0.0;
    }

    double pen = r_sum - dist;
    r.depth_x = pen * std::abs(r.normal.x);
    r.depth_y = pen * std::abs(r.normal.y);
    return r;
}

OverlapResult overlap_aabb_circle_mtv(
    const AABB& box,
    const vector2d& center,
    double radius
) noexcept {
    double cx = std::max(box.min.x, std::min(center.x, box.max.x));
    double cy = std::max(box.min.y, std::min(center.y, box.max.y));
    double dx = center.x - cx;
    double dy = center.y - cy;
    double dist_sq = dx * dx + dy * dy;
    if (dist_sq > radius * radius) return {};
    OverlapResult r;
    r.hit = true;
    double dist = std::sqrt(dist_sq);

    if (dist > constants::middle_epsilon()) {
        r.normal = { dx / dist, dy / dist };
        double pen = radius - dist;
        r.depth_x = pen * std::abs(r.normal.x);
        r.depth_y = pen * std::abs(r.normal.y);
    } else {
        double left   = center.x - box.min.x;
        double right  = box.max.x - center.x;
        double top    = center.y - box.min.y;
        double bottom = box.max.y - center.y;
        double min_d  = std::min({ left, right, top, bottom });

        if (min_d == left)        r.normal = { -1.0,  0.0 };
        else if (min_d == right)  r.normal = {  1.0,  0.0 };
        else if (min_d == top)    r.normal = {  0.0, -1.0 };
        else                      r.normal = {  0.0,  1.0 };

        r.depth_x = (min_d + radius) * std::abs(r.normal.x);
        r.depth_y = (min_d + radius) * std::abs(r.normal.y);
    }

    return r;
}

} // namespace physics
} // namespace fizmo
