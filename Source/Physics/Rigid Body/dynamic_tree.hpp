#ifndef FIZMO_PHYSICS_DYNAMIC_TREE_HPP
#define FIZMO_PHYSICS_DYNAMIC_TREE_HPP

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace fizmo {
namespace physics {

template <class Box>
struct BoxTraits;

template <class Box, class Item>
class DynamicTree {
public:
    static constexpr std::int32_t NONE = -1;

    DynamicTree() = default;

    std::int32_t insert(const Box& box, Item item) {
        const std::int32_t leaf = allocate();
        Node& n = m_nodes[static_cast<std::size_t>(leaf)];
        n.box = box;
        n.item = item;
        n.height = 0;
        n.moved = true;
        insert_leaf(leaf);
        ++m_leaves;
        return leaf;
    }

    void remove(std::int32_t proxy) {
        if (!valid(proxy)) return;
        remove_leaf(proxy);
        release(proxy);
        --m_leaves;
    }

    bool move(std::int32_t proxy, const Box& tight, double margin, const Box& displaced) {
        if (!valid(proxy)) return false;
        Node& n = m_nodes[static_cast<std::size_t>(proxy)];
        if (BoxTraits<Box>::contains(n.box, tight) && BoxTraits<Box>::contains(n.box, displaced)) {
            const Box huge = BoxTraits<Box>::fatten(tight, margin * 4.0);
            if (BoxTraits<Box>::contains(huge, n.box)) return false;
        }
        remove_leaf(proxy);
        m_nodes[static_cast<std::size_t>(proxy)].box = BoxTraits<Box>::merge(BoxTraits<Box>::fatten(tight, margin), displaced);
        insert_leaf(proxy);
        m_nodes[static_cast<std::size_t>(proxy)].moved = true;
        return true;
    }

    void set_item(std::int32_t proxy, Item item) {
        if (valid(proxy)) m_nodes[static_cast<std::size_t>(proxy)].item = item;
    }

    bool valid(std::int32_t proxy) const noexcept {
        return proxy >= 0 && static_cast<std::size_t>(proxy) < m_nodes.size() && m_nodes[static_cast<std::size_t>(proxy)].height == 0 && !m_nodes[static_cast<std::size_t>(proxy)].free;
    }

    const Box& box(std::int32_t proxy) const { return m_nodes[static_cast<std::size_t>(proxy)].box; }
    Item item(std::int32_t proxy) const { return m_nodes[static_cast<std::size_t>(proxy)].item; }
    bool moved(std::int32_t proxy) const { return valid(proxy) && m_nodes[static_cast<std::size_t>(proxy)].moved; }
    void clear_moved(std::int32_t proxy) {
        if (valid(proxy)) m_nodes[static_cast<std::size_t>(proxy)].moved = false;
    }
    void mark_moved(std::int32_t proxy) {
        if (valid(proxy)) m_nodes[static_cast<std::size_t>(proxy)].moved = true;
    }

    template <class Fn>
    void query(const Box& region, Fn&& visit) const {
        if (m_root == NONE) return;
        std::vector<std::int32_t>& stack = m_stack;
        stack.clear();
        stack.push_back(m_root);
        while (!stack.empty()) {
            const std::int32_t id = stack.back();
            stack.pop_back();
            const Node& n = m_nodes[static_cast<std::size_t>(id)];
            if (!BoxTraits<Box>::overlaps(n.box, region)) continue;
            if (n.height == 0) {
                if (!visit(id, n.item)) return;
            } else {
                stack.push_back(n.left);
                stack.push_back(n.right);
            }
        }
    }

    template <class Overlaps, class Fn>
    void query_shape(Overlaps&& node_test, Fn&& visit) const {
        if (m_root == NONE) return;
        std::vector<std::int32_t> stack;
        stack.push_back(m_root);
        while (!stack.empty()) {
            const std::int32_t id = stack.back();
            stack.pop_back();
            const Node& n = m_nodes[static_cast<std::size_t>(id)];
            if (!node_test(n.box)) continue;
            if (n.height == 0) {
                if (!visit(id, n.item)) return;
            } else {
                stack.push_back(n.left);
                stack.push_back(n.right);
            }
        }
    }

    template <class Fn>
    void for_each_leaf(Fn&& fn) const {
        for (std::size_t i = 0; i < m_nodes.size(); ++i) {
            const Node& n = m_nodes[i];
            if (!n.free && n.height == 0) fn(static_cast<std::int32_t>(i), n.item);
        }
    }

    template <class Fn>
    void find_pairs(Fn&& on_pair) const {
        if (m_root == NONE) return;
        std::vector<std::int32_t> stack;
        for (std::size_t i = 0; i < m_nodes.size(); ++i) {
            const Node& leaf = m_nodes[i];
            if (leaf.free || leaf.height != 0) continue;
            const std::int32_t self = static_cast<std::int32_t>(i);
            stack.clear();
            stack.push_back(m_root);
            while (!stack.empty()) {
                const std::int32_t id = stack.back();
                stack.pop_back();
                const Node& n = m_nodes[static_cast<std::size_t>(id)];
                if (!BoxTraits<Box>::overlaps(n.box, leaf.box)) continue;
                if (n.height == 0) {
                    if (id == self) continue;
                    const bool both_moved = leaf.moved && n.moved;
                    if (!leaf.moved && !n.moved) continue;
                    if (both_moved && id < self) continue;
                    on_pair(leaf.item, n.item);
                } else {
                    stack.push_back(n.left);
                    stack.push_back(n.right);
                }
            }
        }
    }

    template <class Fn>
    void find_all_pairs(Fn&& on_pair) const {
        if (m_root == NONE) return;
        std::vector<std::int32_t> stack;
        for (std::size_t i = 0; i < m_nodes.size(); ++i) {
            const Node& leaf = m_nodes[i];
            if (leaf.free || leaf.height != 0) continue;
            const std::int32_t self = static_cast<std::int32_t>(i);
            stack.clear();
            stack.push_back(m_root);
            while (!stack.empty()) {
                const std::int32_t id = stack.back();
                stack.pop_back();
                const Node& n = m_nodes[static_cast<std::size_t>(id)];
                if (!BoxTraits<Box>::overlaps(n.box, leaf.box)) continue;
                if (n.height == 0) {
                    if (id <= self) continue;
                    on_pair(leaf.item, n.item);
                } else {
                    stack.push_back(n.left);
                    stack.push_back(n.right);
                }
            }
        }
    }

    void shift(const typename BoxTraits<Box>::Vector& offset) {
        for (Node& n : m_nodes) {
            if (!n.free) n.box = BoxTraits<Box>::translate(n.box, offset);
        }
    }

    void clear() {
        m_nodes.clear();
        m_free = NONE;
        m_root = NONE;
        m_leaves = 0;
    }

    std::size_t size() const noexcept { return m_leaves; }
    int height() const noexcept { return m_root == NONE ? 0 : m_nodes[static_cast<std::size_t>(m_root)].height; }
    std::size_t node_capacity() const noexcept { return m_nodes.size(); }

    double area_ratio() const {
        if (m_root == NONE) return 0.0;
        const double root = BoxTraits<Box>::cost(m_nodes[static_cast<std::size_t>(m_root)].box);
        if (root <= 0.0) return 0.0;
        double total = 0.0;
        for (const Node& n : m_nodes) {
            if (!n.free) total += BoxTraits<Box>::cost(n.box);
        }
        return total / root;
    }

private:
    struct Node {
        Box box{};
        Item item{};
        std::int32_t parent = NONE;
        std::int32_t left = NONE;
        std::int32_t right = NONE;
        std::int32_t next = NONE;
        int height = 0;
        bool free = false;
        bool moved = false;
    };

    std::int32_t allocate() {
        if (m_free != NONE) {
            const std::int32_t id = m_free;
            Node& n = m_nodes[static_cast<std::size_t>(id)];
            m_free = n.next;
            n = Node{};
            return id;
        }
        m_nodes.push_back(Node{});
        return static_cast<std::int32_t>(m_nodes.size() - 1);
    }

    void release(std::int32_t id) {
        Node& n = m_nodes[static_cast<std::size_t>(id)];
        n.free = true;
        n.height = -1;
        n.next = m_free;
        m_free = id;
    }

    Node& node(std::int32_t id) { return m_nodes[static_cast<std::size_t>(id)]; }

    void insert_leaf(std::int32_t leaf) {
        if (m_root == NONE) {
            m_root = leaf;
            node(leaf).parent = NONE;
            return;
        }
        const Box leaf_box = node(leaf).box;
        std::int32_t index = m_root;
        while (node(index).height > 0) {
            const std::int32_t l = node(index).left;
            const std::int32_t r = node(index).right;
            const double area = BoxTraits<Box>::cost(node(index).box);
            const double combined = BoxTraits<Box>::cost(BoxTraits<Box>::merge(node(index).box, leaf_box));
            const double cost = 2.0 * combined;
            const double inherit = 2.0 * (combined - area);
            auto child_cost = [&](std::int32_t c) {
                const Box merged = BoxTraits<Box>::merge(leaf_box, node(c).box);
                if (node(c).height == 0) return BoxTraits<Box>::cost(merged) + inherit;
                return BoxTraits<Box>::cost(merged) - BoxTraits<Box>::cost(node(c).box) + inherit;
            };
            const double cl = child_cost(l);
            const double cr = child_cost(r);
            if (cost < cl && cost < cr) break;
            index = cl < cr ? l : r;
        }
        const std::int32_t sibling = index;
        const std::int32_t old_parent = node(sibling).parent;
        const std::int32_t parent = allocate();
        node(parent).parent = old_parent;
        node(parent).box = BoxTraits<Box>::merge(leaf_box, node(sibling).box);
        node(parent).height = node(sibling).height + 1;
        if (old_parent != NONE) {
            if (node(old_parent).left == sibling) node(old_parent).left = parent;
            else node(old_parent).right = parent;
        } else {
            m_root = parent;
        }
        node(parent).left = sibling;
        node(parent).right = leaf;
        node(sibling).parent = parent;
        node(leaf).parent = parent;
        refit(node(leaf).parent);
    }

    void remove_leaf(std::int32_t leaf) {
        if (leaf == m_root) {
            m_root = NONE;
            return;
        }
        const std::int32_t parent = node(leaf).parent;
        const std::int32_t grand = node(parent).parent;
        const std::int32_t sibling = node(parent).left == leaf ? node(parent).right : node(parent).left;
        if (grand != NONE) {
            if (node(grand).left == parent) node(grand).left = sibling;
            else node(grand).right = sibling;
            node(sibling).parent = grand;
            release(parent);
            refit(grand);
        } else {
            m_root = sibling;
            node(sibling).parent = NONE;
            release(parent);
        }
    }

    void refit(std::int32_t index) {
        while (index != NONE) {
            index = balance(index);
            const std::int32_t l = node(index).left;
            const std::int32_t r = node(index).right;
            node(index).height = 1 + std::max(node(l).height, node(r).height);
            node(index).box = BoxTraits<Box>::merge(node(l).box, node(r).box);
            index = node(index).parent;
        }
    }

    std::int32_t balance(std::int32_t a) {
        Node& A = node(a);
        if (A.height < 2) return a;
        const std::int32_t b = A.left;
        const std::int32_t c = A.right;
        const int diff = node(c).height - node(b).height;
        if (diff > 1) return rotate(a, c, b, true);
        if (diff < -1) return rotate(a, b, c, false);
        return a;
    }

    std::int32_t rotate(std::int32_t a, std::int32_t up, std::int32_t other, bool up_is_right) {
        const std::int32_t f = node(up).left;
        const std::int32_t g = node(up).right;
        node(up).left = a;
        node(up).parent = node(a).parent;
        node(a).parent = up;
        if (node(up).parent != NONE) {
            Node& p = node(node(up).parent);
            if (p.left == a) p.left = up;
            else p.right = up;
        } else {
            m_root = up;
        }
        std::int32_t keep = f;
        std::int32_t move = g;
        if (node(f).height <= node(g).height) {
            keep = g;
            move = f;
        }
        node(up).right = keep;
        if (up_is_right) node(a).right = move;
        else node(a).left = move;
        node(move).parent = a;
        const std::int32_t al = node(a).left;
        const std::int32_t ar = node(a).right;
        node(a).box = BoxTraits<Box>::merge(node(al).box, node(ar).box);
        node(a).height = 1 + std::max(node(al).height, node(ar).height);
        node(up).box = BoxTraits<Box>::merge(node(a).box, node(keep).box);
        node(up).height = 1 + std::max(node(a).height, node(keep).height);
        (void)other;
        return up;
    }

    std::vector<Node> m_nodes;
    std::int32_t m_free = NONE;
    std::int32_t m_root = NONE;
    std::size_t m_leaves = 0;
    mutable std::vector<std::int32_t> m_stack;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_DYNAMIC_TREE_HPP
