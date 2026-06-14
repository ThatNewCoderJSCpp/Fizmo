#ifndef FIZMO_OCTREE_HPP
#define FIZMO_OCTREE_HPP

#include "physics_3d.hpp"
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace fizmo {
namespace physics {

inline bool overlap_aabb3d(const AABB3D& a, const AABB3D& b) noexcept {
    return a.overlaps(b);
}

inline bool overlap_spheres(
    const vector3d& c1, double r1,
    const vector3d& c2, double r2
) noexcept {
    vector3d d = c2 - c1;
    double r_sum = r1 + r2;
    return d.magnitude_squared() <= r_sum * r_sum;
}

inline bool overlap_aabb3d_sphere(
    const AABB3D& box,
    const vector3d& center,
    double radius
) noexcept {
    double cx = std::max(box.min.x, std::min(center.x, box.max.x));
    double cy = std::max(box.min.y, std::min(center.y, box.max.y));
    double cz = std::max(box.min.z, std::min(center.z, box.max.z));
    double dx = center.x - cx, dy = center.y - cy, dz = center.z - cz;
    return (dx*dx + dy*dy + dz*dz) <= radius * radius;
}

struct OverlapResult3D {
    bool     hit = false;
    double   depth = 0.0;  // meters (penetration depth along normal)
    vector3d normal{};     // unit push direction (a out of b)
};

inline OverlapResult3D overlap_aabb3d_mtv(const AABB3D& a, const AABB3D& b) noexcept {
    double ox = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    double oy = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    double oz = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);
    if (ox <= 0.0 || oy <= 0.0 || oz <= 0.0) return {};

    OverlapResult3D r;
    r.hit = true;
    double min_o = ox;
    vector3d n = { (a.center().x < b.center().x) ? -1.0 : 1.0, 0.0, 0.0 };

    if (oy < min_o) {
        min_o = oy;
        n = { 0.0, (a.center().y < b.center().y) ? -1.0 : 1.0, 0.0 };
    }
    if (oz < min_o) {
        min_o = oz;
        n = { 0.0, 0.0, (a.center().z < b.center().z) ? -1.0 : 1.0 };
    }

    r.depth  = min_o;
    r.normal = n;
    return r;
}

inline OverlapResult3D overlap_spheres_mtv(
    const vector3d& c1, double r1,
    const vector3d& c2, double r2
) noexcept {
    vector3d d = c1 - c2;
    double dist_sq = d.magnitude_squared();
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

inline OverlapResult3D overlap_aabb3d_sphere_mtv(
    const AABB3D& box,
    const vector3d& center,
    double radius
) noexcept {
    double cx = std::max(box.min.x, std::min(center.x, box.max.x));
    double cy = std::max(box.min.y, std::min(center.y, box.max.y));
    double cz = std::max(box.min.z, std::min(center.z, box.max.z));
    double dx = center.x - cx, dy = center.y - cy, dz = center.z - cz;
    double dist_sq = dx*dx + dy*dy + dz*dz;
    if (dist_sq > radius * radius) return {};

    OverlapResult3D r;
    r.hit = true;
    double dist = std::sqrt(dist_sq);

    if (dist > constants::middle_epsilon()) {
        r.normal = { dx / dist, dy / dist, dz / dist };
        r.depth  = radius - dist;
    } else {
        double dists[6] = {
            center.x - box.min.x,  box.max.x - center.x,
            center.y - box.min.y,  box.max.y - center.y,
            center.z - box.min.z,  box.max.z - center.z
        };
        vector3d normals[6] = {
            {-1, 0, 0}, { 1, 0, 0},
            { 0,-1, 0}, { 0, 1, 0},
            { 0, 0,-1}, { 0, 0, 1}
        };
        int best = 0;
        for (int i = 1; i < 6; ++i) if (dists[i] < dists[best]) best = i;
        r.normal = normals[best];
        r.depth  = dists[best] + radius;
    }

    return r;
}

template <typename T>
class Octree {
public:
    struct Entry {
        T*     item;
        AABB3D aabb;
    };

    Octree() noexcept = default;

    explicit Octree(
        const AABB3D& bounds,
        std::size_t   max_per_node = 8,
        std::size_t   max_depth    = 6
    ) noexcept
        : m_bounds(bounds),
          m_max_entries(max_per_node),
          m_max_depth(max_depth) {}

    void rebuild(const AABB3D& bounds, T** items, const AABB3D* aabbs, std::size_t n) {
        m_bounds = bounds;
        clear();
        for (std::size_t i = 0; i < n; ++i) insert(items[i], aabbs[i]);
    }

    void insert(T* item, const AABB3D& item_aabb) { insert_impl(item, item_aabb, 0); }

    void clear() noexcept {
        m_entries.clear();
        m_divided = false;
        for (auto& c : m_children) c.reset();
    }

    void query(const AABB3D& region, std::vector<T*>& out) const {
        if (!m_bounds.overlaps(region)) return;
        for (auto& e : m_entries) if (e.aabb.overlaps(region)) out.push_back(e.item);
        if (m_divided) for (auto& c : m_children) c->query(region, out);
    }

    std::vector<T*> query(const AABB3D& region) const {
        std::vector<T*> out;
        query(region, out);
        return out;
    }

    void query_sphere(const vector3d& center, double radius, std::vector<T*>& out) const {
        if (!overlap_aabb3d_sphere(m_bounds, center, radius)) return;
        for (auto& e : m_entries) if (overlap_aabb3d_sphere(e.aabb, center, radius)) out.push_back(e.item);
        if (m_divided) for (auto& c : m_children) c->query_sphere(center, radius, out);
    }

    template <typename Fn>
    void find_pairs(Fn&& on_pair) const {
        std::vector<const Entry*> ancestors;
        find_pairs_impl(std::forward<Fn>(on_pair), ancestors);
    }

    std::size_t count()      const noexcept { return count_impl(); }
    std::size_t depth()      const noexcept { return depth_impl(0); }
    std::size_t node_count() const noexcept { return node_count_impl(); }
    const AABB3D& bounds()   const noexcept { return m_bounds; }

private:
    //   0: -x -y -z    1: +x -y -z
    //   2: -x +y -z    3: +x +y -z
    //   4: -x -y +z    5: +x -y +z
    //   6: -x +y +z    7: +x +y +z
    AABB3D octant_bounds(int q) const noexcept {
        vector3d mid = m_bounds.center();
        vector3d lo, hi;
        lo.x = (q & 1) ? mid.x : m_bounds.min.x;
        hi.x = (q & 1) ? m_bounds.max.x : mid.x;
        lo.y = (q & 2) ? mid.y : m_bounds.min.y;
        hi.y = (q & 2) ? m_bounds.max.y : mid.y;
        lo.z = (q & 4) ? mid.z : m_bounds.min.z;
        hi.z = (q & 4) ? m_bounds.max.z : mid.z;
        return { lo, hi };
    }

    int find_child(const AABB3D& item_aabb) const noexcept {
        vector3d mid = m_bounds.center();
        bool neg_x = item_aabb.max.x <= mid.x;
        bool pos_x = item_aabb.min.x >= mid.x;
        bool neg_y = item_aabb.max.y <= mid.y;
        bool pos_y = item_aabb.min.y >= mid.y;
        bool neg_z = item_aabb.max.z <= mid.z;
        bool pos_z = item_aabb.min.z >= mid.z;
        if (!neg_x && !pos_x) return -1;
        if (!neg_y && !pos_y) return -1;
        if (!neg_z && !pos_z) return -1;
        int idx = 0;
        if (pos_x) idx |= 1;
        if (pos_y) idx |= 2;
        if (pos_z) idx |= 4;
        return idx;
    }

    void subdivide() {
        for (int q = 0; q < 8; ++q) m_children[q] = std::make_unique<Octree>(octant_bounds(q), m_max_entries, m_max_depth - 1);
        m_divided = true;
        std::vector<Entry> keep;

        for (auto& e : m_entries) {
            int q = find_child(e.aabb);

            if (q >= 0) {
                m_children[q]->insert(e.item, e.aabb);
            } else {
                keep.push_back(std::move(e));
            }
        }

        m_entries = std::move(keep);
    }

    void insert_impl(T* item, const AABB3D& item_aabb, std::size_t depth) {
        if (m_divided) {
            int q = find_child(item_aabb);

            if (q >= 0) {
                m_children[q]->insert_impl(item, item_aabb, depth + 1);
                return;
            }

            m_entries.push_back({ item, item_aabb });
            return;
        }

        m_entries.push_back({ item, item_aabb });
        if (m_entries.size() > m_max_entries && depth < m_max_depth) subdivide();
    }

    template <typename Fn>
    void find_pairs_impl(Fn& on_pair, std::vector<const Entry*>& ancestors) const {
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            for (std::size_t j = i + 1; j < m_entries.size(); ++j) if (m_entries[i].aabb.overlaps(m_entries[j].aabb)) on_pair(m_entries[i].item, m_entries[j].item);
            for (auto* anc : ancestors) if (m_entries[i].aabb.overlaps(anc->aabb)) on_pair(m_entries[i].item, anc->item);
        }

        if (m_divided) {
            std::size_t old = ancestors.size();
            for (auto& e : m_entries) ancestors.push_back(&e);
            for (auto& c : m_children) c->find_pairs_impl(on_pair, ancestors);
            ancestors.resize(old);
        }
    }

    std::size_t count_impl() const noexcept {
        std::size_t n = m_entries.size();
        if (m_divided) for (auto& c : m_children) n += c->count_impl();
        return n;
    }

    std::size_t depth_impl(std::size_t d) const noexcept {
        if (!m_divided) return d;
        std::size_t mx = d;
        for (auto& c : m_children) mx = std::max(mx, c->depth_impl(d + 1));
        return mx;
    }

    std::size_t node_count_impl() const noexcept {
        std::size_t n = 1;
        if (m_divided) for (auto& c : m_children) n += c->node_count_impl();
        return n;
    }

private:
    AABB3D      m_bounds{};
    std::size_t m_max_entries = 8;
    std::size_t m_max_depth   = 6;

    std::vector<Entry>                     m_entries;
    std::array<std::unique_ptr<Octree>, 8> m_children;
    bool                                   m_divided = false;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_OCTREE_HPP