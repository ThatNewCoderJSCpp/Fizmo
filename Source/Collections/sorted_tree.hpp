#ifndef FIZMO_COLLECTIONS_SORTED_TREE_HPP
#define FIZMO_COLLECTIONS_SORTED_TREE_HPP

#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "iterator.hpp"

namespace fizmo {
namespace collections {
namespace detail {

template <class Value>
struct AvlNode {
    AvlNode*    parent = nullptr;
    AvlNode*    left = nullptr;
    AvlNode*    right = nullptr;
    std::size_t size = 1;
    int         height = 1;
    Value       value;

    template <class... Args>
    explicit AvlNode(Args&&... args) : value(std::forward<Args>(args)...) {}
};

template <class Key, class Value, class KeyOf, class Compare, bool Multi>
class AvlTree {
public:
    using Node      = AvlNode<Value>;
    using size_type = std::size_t;

    using iterator       = NodeIterator<AvlTree, Node, Value, false>;
    using const_iterator = NodeIterator<AvlTree, Node, Value, true>;

    AvlTree() = default;
    explicit AvlTree(const Compare& c) : m_cmp(c) {}

    AvlTree(const AvlTree& o) : m_cmp(o.m_cmp) { m_root = clone(o.m_root, nullptr); }

    AvlTree(AvlTree&& o) noexcept : m_root(o.m_root), m_cmp(std::move(o.m_cmp)) { o.m_root = nullptr; }

    AvlTree& operator=(const AvlTree& o) {
        if (this != &o) {
            AvlTree t(o);
            swap(t);
        }

        return *this;
    }

    AvlTree& operator=(AvlTree&& o) noexcept {
        if (this != &o) {
            clear();
            m_root = o.m_root;
            m_cmp = std::move(o.m_cmp);
            o.m_root = nullptr;
        }

        return *this;
    }

    ~AvlTree() { clear(); }

    void swap(AvlTree& o) noexcept {
        std::swap(m_root, o.m_root);
        std::swap(m_cmp, o.m_cmp);
    }

    size_type size() const noexcept { return sz(m_root); }
    bool empty() const noexcept { return m_root == nullptr; }
    const Compare& compare() const noexcept { return m_cmp; }

    iterator begin() noexcept { return iterator(min_node(m_root), this); }
    iterator end() noexcept { return iterator(nullptr, this); }
    const_iterator begin() const noexcept { return const_iterator(min_node(m_root), this); }
    const_iterator end() const noexcept { return const_iterator(nullptr, this); }

    iterator make_iterator(Node* n) noexcept { return iterator(n, this); }
    const_iterator make_iterator(Node* n) const noexcept { return const_iterator(n, this); }

    void clear() noexcept {
        destroy(m_root);
        m_root = nullptr;
    }

    template <class K>
    Node* lower_bound(const K& key) const {
        Node* n = m_root;
        Node* r = nullptr;

        while (n) {
            if (m_cmp(KeyOf::get(n->value), key)) {
                n = n->right;
            } else {
                r = n;
                n = n->left;
            }
        }

        return r;
    }

    template <class K>
    Node* upper_bound(const K& key) const {
        Node* n = m_root;
        Node* r = nullptr;

        while (n) {
            if (m_cmp(key, KeyOf::get(n->value))) {
                r = n;
                n = n->left;
            } else {
                n = n->right;
            }
        }

        return r;
    }

    template <class K>
    Node* floor_node(const K& key) const {
        Node* n = m_root;
        Node* r = nullptr;

        while (n) {
            if (m_cmp(key, KeyOf::get(n->value))) {
                n = n->left;
            } else {
                r = n;
                n = n->right;
            }
        }

        return r;
    }

    template <class K>
    Node* below_node(const K& key) const {
        Node* n = m_root;
        Node* r = nullptr;

        while (n) {
            if (m_cmp(KeyOf::get(n->value), key)) {
                r = n;
                n = n->right;
            } else {
                n = n->left;
            }
        }

        return r;
    }

    template <class K>
    Node* find(const K& key) const {
        Node* n = lower_bound(key);
        return (n && !m_cmp(key, KeyOf::get(n->value))) ? n : nullptr;
    }

    template <class K>
    size_type count(const K& key) const {
        if (!Multi) return find(key) ? 1 : 0;
        return rank_of_bound(upper_bound(key)) - rank_of_bound(lower_bound(key));
    }

    template <class... Args>
    std::pair<Node*, bool> emplace(Args&&... args) {
        Node* fresh = new Node(std::forward<Args>(args)...);

        try {
            return insert_node(fresh);
        } catch (...) {
            delete fresh;
            throw;
        }
    }

    template <class K, class Make>
    std::pair<Node*, bool> find_or_emplace(const K& key, Make&& make) {
        static_assert(!Multi, "find_or_emplace requires a unique-key tree");
        Node* existing = find(key);
        if (existing) return { existing, false };
        return emplace(make());
    }

    void erase(Node* z) noexcept {
        Node* start;

        if (z->left && z->right) {
            Node* y = min_node(z->right);

            if (y->parent == z) {
                start = y;
            } else {
                start = y->parent;
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }

            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
        } else {
            Node* child = z->left ? z->left : z->right;
            start = z->parent;
            transplant(z, child);
        }

        delete z;
        rebalance(start);
    }

    template <class K>
    size_type erase_key(const K& key) {
        size_type n = 0;

        for (;;) {
            Node* f = find(key);
            if (!f) break;
            erase(f);
            ++n;
            if (!Multi) break;
        }

        return n;
    }

    size_type rank(const Node* n) const noexcept {
        size_type r = sz(n->left);

        while (n->parent) {
            if (n == n->parent->right) r += sz(n->parent->left) + 1;
            n = n->parent;
        }

        return r;
    }

    size_type rank_of_bound(const Node* n) const noexcept { return n ? rank(n) : size(); }

    Node* select(size_type i) const noexcept {
        Node* n = m_root;

        while (n) {
            const size_type ls = sz(n->left);

            if (i < ls) {
                n = n->left;
            } else if (i == ls) {
                return n;
            } else {
                i -= ls + 1;
                n = n->right;
            }
        }

        return nullptr;
    }

    Node* first() const noexcept { return min_node(m_root); }
    Node* last() const noexcept { return max_node(m_root); }

    static Node* min_node(Node* n) noexcept {
        if (!n) return nullptr;
        while (n->left) n = n->left;
        return n;
    }

    static Node* max_node(Node* n) noexcept {
        if (!n) return nullptr;
        while (n->right) n = n->right;
        return n;
    }

    static Node* successor(Node* n) noexcept {
        if (n->right) return min_node(n->right);
        Node* p = n->parent;

        while (p && n == p->right) {
            n = p;
            p = p->parent;
        }

        return p;
    }

    static Value& node_value(Node* n) noexcept { return n->value; }
    static Node* next_node(Node* n, const AvlTree*) noexcept { return successor(n); }
    static Node* prev_node(Node* n, const AvlTree* t) noexcept { return n ? predecessor(n) : max_node(t->m_root); }

    static Node* predecessor(Node* n) noexcept {
        if (n->left) return max_node(n->left);
        Node* p = n->parent;

        while (p && n == p->left) {
            n = p;
            p = p->parent;
        }

        return p;
    }

private:
    Node*   m_root = nullptr;
    Compare m_cmp;

    static size_type sz(const Node* n) noexcept { return n ? n->size : 0; }
    static int ht(const Node* n) noexcept { return n ? n->height : 0; }

    static void update(Node* n) noexcept {
        n->size = sz(n->left) + sz(n->right) + 1;
        n->height = 1 + (ht(n->left) > ht(n->right) ? ht(n->left) : ht(n->right));
    }

    std::pair<Node*, bool> insert_node(Node* fresh) {
        Node* parent = nullptr;
        Node* n = m_root;
        bool left = false;
        const auto& key = KeyOf::get(fresh->value);

        while (n) {
            parent = n;

            if (m_cmp(key, KeyOf::get(n->value))) {
                n = n->left;
                left = true;
            } else if (!Multi && !m_cmp(KeyOf::get(n->value), key)) {
                delete fresh;
                return { n, false };
            } else {
                n = n->right;
                left = false;
            }
        }

        fresh->parent = parent;

        if (!parent) {
            m_root = fresh;
        } else if (left) {
            parent->left = fresh;
        } else {
            parent->right = fresh;
        }

        rebalance(parent);
        return { fresh, true };
    }

    void transplant(Node* u, Node* v) noexcept {
        if (!u->parent) {
            m_root = v;
        } else if (u == u->parent->left) {
            u->parent->left = v;
        } else {
            u->parent->right = v;
        }

        if (v) v->parent = u->parent;
    }

    Node* rotate_left(Node* x) noexcept {
        Node* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;
        transplant(x, y);
        y->left = x;
        x->parent = y;
        update(x);
        update(y);
        return y;
    }

    Node* rotate_right(Node* x) noexcept {
        Node* y = x->left;
        x->left = y->right;
        if (y->right) y->right->parent = x;
        transplant(x, y);
        y->right = x;
        x->parent = y;
        update(x);
        update(y);
        return y;
    }

    void rebalance(Node* n) noexcept {
        while (n) {
            update(n);
            const int bal = ht(n->left) - ht(n->right);

            if (bal > 1) {
                if (ht(n->left->left) < ht(n->left->right)) rotate_left(n->left);
                n = rotate_right(n);
            } else if (bal < -1) {
                if (ht(n->right->right) < ht(n->right->left)) rotate_right(n->right);
                n = rotate_left(n);
            }

            n = n->parent;
        }
    }

    static void destroy(Node* n) noexcept {
        while (n) {
            if (n->left) {
                Node* l = n->left;
                n->left = l->right;
                l->right = n;
                n = l;
            } else {
                Node* r = n->right;
                delete n;
                n = r;
            }
        }
    }

    static Node* clone(const Node* src, Node* parent) {
        if (!src) return nullptr;
        Node* n = new Node(src->value);
        n->parent = parent;
        n->size = src->size;
        n->height = src->height;

        try {
            n->left = clone(src->left, n);
            n->right = clone(src->right, n);
        } catch (...) {
            destroy(n->left);
            destroy(n->right);
            delete n;
            throw;
        }

        return n;
    }
};

} // namespace detail
} // namespace collections
} // namespace fizmo

#endif // FIZMO_COLLECTIONS_SORTED_TREE_HPP
