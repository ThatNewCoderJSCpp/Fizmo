#ifndef FIZMO_QUADTREE_HPP
#define FIZMO_QUADTREE_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include "physics_2d.hpp"

namespace fizmo {
namespace physics {

inline bool overlap_aabb(const AABB& a, const AABB& b) noexcept { return a.overlaps(b); }

bool overlap_circles(
    const vector2d& c1, double r1,
    const vector2d& c2, double r2
) noexcept;

bool overlap_aabb_circle(
    const AABB& box,
    const vector2d& center,
    double radius
) noexcept;

struct OverlapResult {
    bool     hit = false;
    double   depth_x = 0.0; 
    double   depth_y = 0.0;
    vector2d normal{};      
};

OverlapResult overlap_aabb_mtv(const AABB& a, const AABB& b) noexcept;

OverlapResult overlap_circles_mtv(
    const vector2d& c1, double r1,
    const vector2d& c2, double r2
) noexcept;

OverlapResult overlap_aabb_circle_mtv(
    const AABB& box,
    const vector2d& center,
    double radius
) noexcept;

template <typename T>
class Quadtree {
public:
    struct Entry {
        T*   item;
        AABB aabb;
    };

    static constexpr double TIGHT = 1.0;
    static constexpr double RECOMMENDED_LOOSENESS = 2.0;

    Quadtree() { reset_root(); }

    explicit Quadtree(
        const AABB&  bounds,
        std::size_t  max_per_node = 8,
        std::size_t  max_depth    = 6,
        double       looseness    = TIGHT
    ) : m_max_entries(max_per_node), m_max_depth(max_depth), m_looseness(std::max(looseness, TIGHT)) {
        reset_root();
        m_nodes[ROOT].bounds = bounds;
        m_nodes[ROOT].loose  = loosen(bounds);
    }

    void configure(std::size_t max_per_node, std::size_t max_depth, double looseness) {
        m_max_entries = max_per_node;
        m_max_depth   = max_depth;
        m_looseness   = std::max(looseness, TIGHT);
        if (m_scratch.size() < m_max_depth + 2) m_scratch.resize(m_max_depth + 2);
    }

    void rebuild(const AABB& bounds, T** items, const AABB* aabbs, std::size_t n) {
        clear();
        m_nodes[ROOT].bounds = bounds;
        m_nodes[ROOT].loose  = loosen(bounds);
        for (std::size_t i = 0; i < n; ++i) insert(items[i], aabbs[i]);
    }

    void insert(T* item, const AABB& item_aabb) {
        std::uint32_t n = ROOT;

        for (;;) {
            Node& node = m_nodes[n];

            if (node.first_child != NO_CHILD) {
                const int q = find_child(node, item_aabb);
                if (q >= 0) { n = node.first_child + static_cast<std::uint32_t>(q); continue; }
                node.entries.push_back({ item, item_aabb });
                return;
            }

            node.entries.push_back({ item, item_aabb });
            if (node.entries.size() > m_max_entries && node.depth < m_max_depth) subdivide(n);
            return;
        }
    }

    void clear() {
        if (m_scratch.size() < m_max_depth + 2) m_scratch.resize(m_max_depth + 2);

        for (std::size_t i = 0; i < m_used; ++i) {
            m_nodes[i].entries.clear();
            m_nodes[i].first_child = NO_CHILD;
        }

        m_used = 1;
    }

    void query(const AABB& region, std::vector<T*>& out) const { query_node(ROOT, region, out); }

    std::vector<T*> query(const AABB& region) const {
        std::vector<T*> out;
        query(region, out);
        return out;
    }

    template <typename Fn>
    void find_pairs(Fn&& on_pair) const {
        if (is_loose()) {
            for_each_entry(ROOT, [&](const Entry& e) { pair_with(ROOT, e, on_pair); });
            return;
        }

        m_ancestors.clear();
        find_pairs_impl(ROOT, on_pair);
    }

    std::size_t count()        const noexcept { return count_impl(ROOT); }
    std::size_t depth()        const noexcept { return depth_impl(ROOT); }
    std::size_t node_count()   const noexcept { return m_used; }
    std::size_t pooled_nodes() const noexcept { return m_nodes.size(); }
    const AABB& bounds()       const noexcept { return m_nodes[ROOT].bounds; }
    const AABB& loose_bounds() const noexcept { return m_nodes[ROOT].loose; }
    std::size_t max_per_node() const noexcept { return m_max_entries; }
    std::size_t max_depth()    const noexcept { return m_max_depth; }
    double looseness()         const noexcept { return m_looseness; }
    bool   is_loose()          const noexcept { return m_looseness > TIGHT; }

private:
    static constexpr std::uint32_t ROOT     = 0;
    static constexpr std::uint32_t NO_CHILD = 0xFFFFFFFFu;
    static constexpr int           CHILDREN = 4;

    struct Node {
        AABB               bounds{};
        AABB               loose{};
        std::vector<Entry> entries;
        std::uint32_t      first_child = NO_CHILD;
        std::size_t        depth = 0;
    };

    void reset_root() {
        m_scratch.resize(m_max_depth + 2);
        m_nodes.resize(std::max<std::size_t>(m_nodes.size(), 1));
        m_used = 1;
        m_nodes[ROOT].first_child = NO_CHILD;
        m_nodes[ROOT].depth = 0;
    }

    AABB loosen(const AABB& b) const noexcept {
        const vector2d c = b.center();
        const vector2d h = b.extents() * m_looseness;
        return { c - h, c + h };
    }

    static AABB quadrant_bounds(const AABB& b, int q) noexcept {
        const vector2d mid = b.center();
        vector2d lo = b.min, hi = b.max;
        if (q & 1) lo.x = mid.x; else hi.x = mid.x;
        if (q & 2) lo.y = mid.y; else hi.y = mid.y;
        return { lo, hi };
    }

    int find_child(const Node& node, const AABB& item_aabb) const noexcept {
        const vector2d mid = node.bounds.center();

        if (is_loose()) {
            const vector2d c = item_aabb.center();
            const int q = (c.x >= mid.x ? 1 : 0) | (c.y >= mid.y ? 2 : 0);
            return loosen(quadrant_bounds(node.bounds, q)).contains(item_aabb) ? q : -1;
        }

        if (!node.bounds.contains(item_aabb)) return -1;
        int q = 0;
        if (item_aabb.min.x > mid.x) q |= 1; else if (!(item_aabb.max.x < mid.x)) return -1;
        if (item_aabb.min.y > mid.y) q |= 2; else if (!(item_aabb.max.y < mid.y)) return -1;
        return q;
    }

    void subdivide(std::uint32_t n) {
        const std::uint32_t first = static_cast<std::uint32_t>(m_used);
        m_used += CHILDREN;
        if (m_nodes.size() < m_used) m_nodes.resize(m_used); 
        const AABB parent = m_nodes[n].bounds;
        const std::size_t depth = m_nodes[n].depth + 1;

        for (int q = 0; q < CHILDREN; ++q) {
            Node& c = m_nodes[first + static_cast<std::uint32_t>(q)];
            c.bounds = quadrant_bounds(parent, q);
            c.loose  = loosen(c.bounds);
            c.entries.clear();
            c.first_child = NO_CHILD;
            c.depth = depth;
        }

        m_nodes[n].first_child = first;
        std::vector<Entry>& moving = m_scratch[depth];
        moving.assign(m_nodes[n].entries.begin(), m_nodes[n].entries.end());
        m_nodes[n].entries.clear();

        for (const Entry& e : moving) {
            const int q = find_child(m_nodes[n], e.aabb);
            if (q >= 0) insert_into(first + static_cast<std::uint32_t>(q), e);
            else m_nodes[n].entries.push_back(e);
        }

        moving.clear();
    }

    void insert_into(std::uint32_t n, const Entry& e) {
        Node& node = m_nodes[n];
        node.entries.push_back(e);
        if (node.entries.size() > m_max_entries && node.depth < m_max_depth) subdivide(n);
    }

    void query_node(std::uint32_t n, const AABB& region, std::vector<T*>& out) const {
        const Node& node = m_nodes[n];
        if (n != ROOT && !node.loose.overlaps(region)) return; 
        for (const Entry& e : node.entries) if (e.aabb.overlaps(region)) out.push_back(e.item);
        if (node.first_child == NO_CHILD) return;
        for (int q = 0; q < CHILDREN; ++q) query_node(node.first_child + static_cast<std::uint32_t>(q), region, out);
    }

    template <typename Fn>
    void for_each_entry(std::uint32_t n, Fn&& fn) const {
        const Node& node = m_nodes[n];
        for (const Entry& e : node.entries) fn(e);
        if (node.first_child == NO_CHILD) return;
        for (int q = 0; q < CHILDREN; ++q) for_each_entry(node.first_child + static_cast<std::uint32_t>(q), fn);
    }

    template <typename Fn>
    void pair_with(std::uint32_t n, const Entry& e, Fn& on_pair) const {
        const Node& node = m_nodes[n];
        if (n != ROOT && !node.loose.overlaps(e.aabb)) return; 
        const std::less<const Entry*> before;

        for (const Entry& other : node.entries)
            if (before(&e, &other) && other.aabb.overlaps(e.aabb)) on_pair(e.item, other.item);

        if (node.first_child == NO_CHILD) return;
        for (int q = 0; q < CHILDREN; ++q) pair_with(node.first_child + static_cast<std::uint32_t>(q), e, on_pair);
    }

    template <typename Fn>
    void find_pairs_impl(std::uint32_t n, Fn& on_pair) const {
        const Node& node = m_nodes[n];

        for (std::size_t i = 0; i < node.entries.size(); ++i) {
            for (std::size_t j = i + 1; j < node.entries.size(); ++j)
                if (node.entries[i].aabb.overlaps(node.entries[j].aabb)) on_pair(node.entries[i].item, node.entries[j].item);

            for (const Entry* anc : m_ancestors)
                if (node.entries[i].aabb.overlaps(anc->aabb)) on_pair(node.entries[i].item, anc->item);
        }

        if (node.first_child == NO_CHILD) return;
        const std::size_t old = m_ancestors.size();
        for (const Entry& e : node.entries) m_ancestors.push_back(&e);
        for (int q = 0; q < CHILDREN; ++q) find_pairs_impl(node.first_child + static_cast<std::uint32_t>(q), on_pair);
        m_ancestors.resize(old);
    }

    std::size_t count_impl(std::uint32_t n) const noexcept {
        const Node& node = m_nodes[n];
        std::size_t c = node.entries.size();
        if (node.first_child != NO_CHILD)
            for (int q = 0; q < CHILDREN; ++q) c += count_impl(node.first_child + static_cast<std::uint32_t>(q));
        return c;
    }

    std::size_t depth_impl(std::uint32_t n) const noexcept {
        const Node& node = m_nodes[n];
        if (node.first_child == NO_CHILD) return node.depth;
        std::size_t mx = node.depth;
        for (int q = 0; q < CHILDREN; ++q) mx = std::max(mx, depth_impl(node.first_child + static_cast<std::uint32_t>(q)));
        return mx;
    }

    std::vector<Node>                  m_nodes;
    std::size_t                        m_used        = 1;
    std::size_t                        m_max_entries = 8;
    std::size_t                        m_max_depth   = 6;
    double                             m_looseness   = TIGHT;
    std::vector<std::vector<Entry>>    m_scratch;
    mutable std::vector<const Entry*>  m_ancestors;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_QUADTREE_HPP