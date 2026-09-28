#ifndef FIZMO_OCTREE_HPP
#define FIZMO_OCTREE_HPP

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "physics_3d.hpp"

namespace fizmo {
namespace physics {

inline bool overlap_aabb(const AABB3D& a, const AABB3D& b) noexcept { return a.overlaps(b); }

inline bool overlap_spheres(
    const vector3d& c1, double r1,
    const vector3d& c2, double r2
) noexcept {
    double r_sum = r1 + r2;
    return vec3::length_squared(c2 - c1) <= r_sum * r_sum;
}

inline bool overlap_aabb_sphere(
    const AABB3D& box,
    const vector3d& center,
    double radius
) noexcept {
    vector3d closest = vec3::max(box.min, vec3::min(center, box.max));
    return vec3::length_squared(center - closest) <= radius * radius;
}

struct OverlapResult3D {
    bool     hit = false;
    double   depth = 0.0; // meters (positive = overlapping)
    vector3d normal{};    // unit push direction (a out of b)
};

inline OverlapResult3D overlap_aabb_mtv(const AABB3D& a, const AABB3D& b) noexcept {
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

inline OverlapResult3D overlap_spheres_mtv(
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

inline OverlapResult3D overlap_aabb_sphere_mtv(
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

template <typename T>
class Octree {
public:
    struct Entry {
        T*     item;
        AABB3D aabb;
    };

    static constexpr double TIGHT = 1.0;
    static constexpr double RECOMMENDED_LOOSENESS = 2.0;

    Octree() noexcept = default;

    explicit Octree(
        const AABB3D& bounds,
        std::size_t   max_per_node = 8,
        std::size_t   max_depth    = 6,
        double        looseness    = TIGHT
    ) noexcept
        : m_bounds(bounds),
          m_max_entries(max_per_node),
          m_max_depth(max_depth),
          m_looseness(std::max(looseness, TIGHT)) { update_loose_bounds(); }

    void rebuild(const AABB3D& bounds, T** items, const AABB3D* aabbs, std::size_t n) {
        clear();
        m_bounds = bounds;
        update_loose_bounds();
        for (std::size_t i = 0; i < n; ++i) insert(items[i], aabbs[i]);
    }

    void insert(T* item, const AABB3D& item_aabb) {
        if (m_divided) {
            int q = find_child(item_aabb);

            if (q >= 0) {
                m_children[q]->insert(item, item_aabb);
                return;
            }

            m_entries.push_back({ item, item_aabb });
            return;
        }

        m_entries.push_back({ item, item_aabb });
        if (m_entries.size() > m_max_entries && m_depth < m_max_depth) subdivide();
    }

    void clear() noexcept {
        m_entries.clear();
        m_divided = false;
        for (auto& c : m_children) c.reset();
    }

    void query(const AABB3D& region, std::vector<T*>& out) const {
        if (!m_loose_bounds.overlaps(region)) return;
        for (auto& e : m_entries) { if (e.aabb.overlaps(region)) out.push_back(e.item); }
        if (m_divided) { for (auto& c : m_children) c->query(region, out); }
    }

    std::vector<T*> query(const AABB3D& region) const {
        std::vector<T*> out;
        query(region, out);
        return out;
    }

    template <typename Fn>
    void find_pairs(Fn&& on_pair) const {
        if (is_loose()) {
            for_each_entry([&](const Entry& e) { pair_with(e, on_pair); });
            return;
        }

        std::vector<const Entry*> ancestors;
        find_pairs_impl(on_pair, ancestors);
    }

    std::size_t count()      const noexcept { return count_impl(); }
    std::size_t depth()      const noexcept { return depth_impl(0); }
    std::size_t node_count() const noexcept { return node_count_impl(); }
    const AABB3D& bounds()       const noexcept { return m_bounds; }
    const AABB3D& loose_bounds() const noexcept { return m_loose_bounds; }
    double looseness()           const noexcept { return m_looseness; }
    bool   is_loose()            const noexcept { return m_looseness > TIGHT; }

private:
    AABB3D loosen(const AABB3D& b) const noexcept {
        const vector3d c = b.center();
        const vector3d h = b.extents() * m_looseness;
        return { c - h, c + h };
    }

    void update_loose_bounds() noexcept { m_loose_bounds = loosen(m_bounds); }

    AABB3D octant_bounds(int q) const noexcept {
        vector3d mid = m_bounds.center();
        vector3d lo = m_bounds.min, hi = m_bounds.max;
        if (q & 1) lo.x = mid.x; else hi.x = mid.x;
        if (q & 2) lo.y = mid.y; else hi.y = mid.y;
        if (q & 4) lo.z = mid.z; else hi.z = mid.z;
        return { lo, hi };
    }

    int find_child(const AABB3D& item_aabb) const noexcept {
        vector3d mid = m_bounds.center();

        if (is_loose()) {
            const vector3d c = item_aabb.center();
            const int q = (c.x >= mid.x ? 1 : 0) | (c.y >= mid.y ? 2 : 0) | (c.z >= mid.z ? 4 : 0);
            return loosen(octant_bounds(q)).contains(item_aabb) ? q : -1;
        }

        int q = 0;
        if (item_aabb.min.x >= mid.x) q |= 1; else if (item_aabb.max.x > mid.x) return -1;
        if (item_aabb.min.y >= mid.y) q |= 2; else if (item_aabb.max.y > mid.y) return -1;
        if (item_aabb.min.z >= mid.z) q |= 4; else if (item_aabb.max.z > mid.z) return -1;
        return q;
    }

    void subdivide() {
        for (int q = 0; q < 8; ++q) {
            m_children[q] = std::make_unique<Octree>(octant_bounds(q), m_max_entries, m_max_depth, m_looseness);
            m_children[q]->m_depth = m_depth + 1;
        }

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

    template <typename Fn>
    void for_each_entry(Fn&& fn) const {
        for (auto& e : m_entries) fn(e);
        if (m_divided) { for (auto& c : m_children) c->for_each_entry(fn); }
    }

    template <typename Fn>
    void pair_with(const Entry& e, Fn& on_pair) const {
        if (!m_loose_bounds.overlaps(e.aabb)) return;
        const std::less<const Entry*> before;

        for (auto& other : m_entries) {
            if (before(&e, &other) && other.aabb.overlaps(e.aabb)) on_pair(e.item, other.item);
        }

        if (m_divided) { for (auto& c : m_children) c->pair_with(e, on_pair); }
    }

    template <typename Fn>
    void find_pairs_impl(
        Fn& on_pair,
        std::vector<const Entry*>& ancestors
    ) const {
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            for (std::size_t j = i + 1; j < m_entries.size(); ++j) {
                if (m_entries[i].aabb.overlaps(m_entries[j].aabb)) on_pair(m_entries[i].item, m_entries[j].item);
            }

            for (auto* anc : ancestors) { if (m_entries[i].aabb.overlaps(anc->aabb)) on_pair(m_entries[i].item, anc->item); }
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
    AABB3D      m_loose_bounds{};
    std::size_t m_max_entries = 8;
    std::size_t m_max_depth   = 6;
    std::size_t m_depth       = 0;
    double      m_looseness   = TIGHT;

    std::vector<Entry>                     m_entries;
    std::array<std::unique_ptr<Octree>, 8> m_children;
    bool                                   m_divided = false;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_OCTREE_HPP