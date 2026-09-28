#ifndef FIZMO_QUADTREE_HPP
#define FIZMO_QUADTREE_HPP

#include "physics_2d.hpp"

namespace fizmo {
namespace physics {

inline bool overlap_aabb(const AABB& a, const AABB& b) noexcept { return a.overlaps(b); }

inline bool overlap_circles(
    const vector2d& c1, double r1,
    const vector2d& c2, double r2
) noexcept {
    double dx = c2.x - c1.x;
    double dy = c2.y - c1.y;
    double r_sum = r1 + r2;
    return (dx * dx + dy * dy) <= r_sum * r_sum;
}

inline bool overlap_aabb_circle(
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

struct OverlapResult {
    bool     hit = false;
    double   depth_x = 0.0; // meters (positive = overlapping)
    double   depth_y = 0.0;
    vector2d normal{};      // unit push direction (a out of b)
};

inline OverlapResult overlap_aabb_mtv(const AABB& a, const AABB& b) noexcept {
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

inline OverlapResult overlap_circles_mtv(
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

inline OverlapResult overlap_aabb_circle_mtv(
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

template <typename T>
class Quadtree {
public:
    struct Entry {
        T*   item;
        AABB aabb;
    };

    Quadtree() noexcept = default;

    explicit Quadtree(
        const AABB&  bounds,
        std::size_t  max_per_node = 8,
        std::size_t  max_depth    = 6
    ) noexcept
        : m_bounds(bounds),
          m_max_entries(max_per_node),
          m_max_depth(max_depth) {}

    void rebuild(const AABB& bounds, T** items, const AABB* aabbs, std::size_t n) {
        m_bounds = bounds;
        clear();
        for (std::size_t i = 0; i < n; ++i) insert(items[i], aabbs[i]);
    }

    void insert(T* item, const AABB& item_aabb) { insert_impl(item, item_aabb, 0); }

    void clear() noexcept {
        m_entries.clear();
        m_divided = false;
        for (auto& c : m_children) c.reset();
    }

    void query(const AABB& region, std::vector<T*>& out) const {
        if (!m_bounds.overlaps(region)) return;
        for (auto& e : m_entries) { if (e.aabb.overlaps(region)) out.push_back(e.item); }
        if (m_divided) { for (auto& c : m_children) c->query(region, out); }
    }

    std::vector<T*> query(const AABB& region) const {
        std::vector<T*> out;
        query(region, out);
        return out;
    }

    template <typename Fn>
    void find_pairs(Fn&& on_pair) const {
        std::vector<const Entry*> ancestors;
        find_pairs_impl(on_pair, ancestors); 
    }

    std::size_t count()     const noexcept { return count_impl(); }
    std::size_t depth()     const noexcept { return depth_impl(0); }
    std::size_t node_count() const noexcept { return node_count_impl(); }
    const AABB& bounds() const noexcept { return m_bounds; }

private:
    AABB quadrant_bounds(int q) const noexcept {
        vector2d mid = m_bounds.center();

        switch (q) {
            case 0: return { m_bounds.min, mid };
            case 1: return { { mid.x, m_bounds.min.y }, { m_bounds.max.x, mid.y } };
            case 2: return { { m_bounds.min.x, mid.y }, { mid.x, m_bounds.max.y } };
            case 3: return { mid, m_bounds.max };
            default: return m_bounds;
        }
    }

    int find_child(const AABB& item_aabb) const noexcept {
        vector2d mid = m_bounds.center();
        bool left   = item_aabb.max.x <= mid.x;
        bool right  = item_aabb.min.x >= mid.x;
        bool top    = item_aabb.max.y <= mid.y;
        bool bottom = item_aabb.min.y >= mid.y;
        if (left  && top)    return 0;
        if (right && top)    return 1;
        if (left  && bottom) return 2;
        if (right && bottom) return 3;
        return -1; 
    }

    void subdivide() {
        for (int q = 0; q < 4; ++q) m_children[q] = std::make_unique<Quadtree>(quadrant_bounds(q), m_max_entries, m_max_depth - 1);
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

    void insert_impl(T* item, const AABB& item_aabb, std::size_t depth) {
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
        if (m_entries.size() > m_max_entries && depth < m_max_depth) { subdivide(); }
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
    AABB        m_bounds{};
    std::size_t m_max_entries = 8;
    std::size_t m_max_depth   = 6;

    std::vector<Entry>                       m_entries;
    std::array<std::unique_ptr<Quadtree>, 4> m_children;
    bool                                     m_divided = false;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_QUADTREE_HPP